use std::f64::consts::LN_10;
use std::ops::{Add, Sub, Mul, Div};
const HBAR: f64 = 197.3269804;
const E2: f64 = 1.43996448;
#[derive(Clone, Copy, Default, Debug)]
struct C { re: f64, im: f64 }
impl C { fn new(re:f64, im:f64)->Self { Self {re,im} } fn abs(self)->f64 { self.re.hypot(self.im) } }
impl Add for C { type Output=Self; fn add(self,b:Self)->Self { Self::new(self.re+b.re,self.im+b.im) } }
impl Sub for C { type Output=Self; fn sub(self,b:Self)->Self { Self::new(self.re-b.re,self.im-b.im) } }
impl Mul for C { type Output=Self; fn mul(self,b:Self)->Self { Self::new(self.re*b.re-self.im*b.im,self.re*b.im+self.im*b.re) } }
impl Mul<f64> for C { type Output=Self; fn mul(self,b:f64)->Self { Self::new(self.re*b,self.im*b) } }
impl Div<f64> for C { type Output=Self; fn div(self,b:f64)->Self { self*(1.0/b) } }
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Request {
    version:u32, kind:u32, l:i32, reserved:u32,
    daughter_z:f64, reduced_mass_mev:f64, q_mev:f64, depth_mev:f64, radius_fm:f64, diffuseness_fm:f64, charge_radius_fm:f64,
    lead_radius_fm:f64, match_radius_fm:f64, step_fm:f64,
}
#[repr(C)]
pub struct ResultPod { version:u32, status:u32, log10_transmission:f64, step_error_decades:f64, match_error_decades:f64, message:[u8;192] }
fn potential(p:&Request,r:f64)->f64 {
    let nuclear = if p.kind==0 { if r<p.radius_fm {-p.depth_mev} else {0.0} } else { -p.depth_mev/(1.0+((r-p.radius_fm)/p.diffuseness_fm).exp()) };
    let k=2.0*p.daughter_z*E2;
    let coulomb=if r<p.charge_radius_fm { k*(3.0-(r/p.charge_radius_fm).powi(2))/(2.0*p.charge_radius_fm) } else { k/r };
    nuclear+coulomb+HBAR.powi(2)*(p.l as f64)*(p.l as f64+1.0)/(2.0*p.reduced_mass_mev*r*r)
}
fn valid(p:&Request)->bool {
    let a=[p.daughter_z,p.reduced_mass_mev,p.q_mev,p.depth_mev,p.radius_fm,p.diffuseness_fm,p.charge_radius_fm,p.lead_radius_fm,p.match_radius_fm,p.step_fm];
    p.version==1 && p.reserved==0 && p.kind<=1 && (0..=100).contains(&p.l) && a.iter().all(|x|x.is_finite()) && p.daughter_z>=0.0 && p.reduced_mass_mev>0.0 && p.q_mev>0.0 && p.depth_mev>=0.0 && p.radius_fm>0.0 && p.diffuseness_fm>0.0 && p.charge_radius_fm>0.0 && p.lead_radius_fm>0.0 && p.match_radius_fm>p.lead_radius_fm && p.step_fm>0.0 && p.q_mev>potential(p,p.lead_radius_fm)
}
// DLMF 33.11 outgoing H+: expand exp(i(rho-eta log(2rho))) in inverse rho.
// Truncate at the least term, rejecting points outside its asymptotic regime.
fn outgoing(rho:f64,eta:f64,l:i32)->Option<C> {
    let mut a=C::new(1.0,0.0); let mut sum=a; let mut prev=1.0;
    let ll=(l as f64)*(l as f64+1.0);
    for n in 0..80 {
        let n=n as f64;
        let factor=C::new(n*(n+1.0)-eta*eta-ll, eta*(2.0*n+1.0));
        let t=a*factor; a=C::new(t.im,-t.re)/(2.0*(n+1.0)*rho);
        let size=a.abs();
        if size>prev { return if prev<1e-12 { Some(sum) } else { None }; }
        sum=sum+a; if size<1e-15*sum.abs() { break; } prev=size;
    }
    let phase=rho-eta*(2.0*rho).ln(); Some(sum*C::new(phase.cos(),phase.sin()))
}
fn solve_once(p:&Request, step:f64, outer:f64, cancel:&dyn Fn()->bool)->Option<f64> {
    let k=(2.0*p.reduced_mass_mev*p.q_mev).sqrt()/HBAR;
    let eta=p.reduced_mass_mev*(2.0*p.daughter_z*E2)/(HBAR*HBAR*k);
    if p.kind==1 && p.depth_mev/(1.0+((outer-p.radius_fm)/p.diffuseness_fm).exp())>1e-12*p.q_mev {return None}
    if outer<=p.charge_radius_fm.max(p.radius_fm) {return None}
    let kin=(2.0*p.reduced_mass_mev*(p.q_mev-potential(p,p.lead_radius_fm))).sqrt()/HBAR;
    propagate(p.lead_radius_fm,outer,step,k,kin,|r|2.0*p.reduced_mass_mev*(potential(p,r)-p.q_mev)/(HBAR*HBAR),|r|outgoing(k*r,eta,p.l),cancel)
}
fn propagate<F:Fn(f64)->f64,G:Fn(f64)->Option<C>>(inner:f64,outer:f64,step:f64,k:f64,kin:f64,f:F,boundary:G,cancel:&dyn Fn()->bool)->Option<f64> {
    let n=((outer-inner)/step).ceil() as usize;
    if n<4 || n>5_000_000 {return None}
    let h=(outer-inner)/(n as f64);
    let mut yp=boundary(outer)?; let mut y=boundary(outer-h)?;
    let mut log_scale=0.0; let mut last=[C::default();5];
    for j in (0..=n-2).rev() {
        if j % 4096 == 0 && cancel() {return None}
        let r=inner+(j as f64)*h;
        let ym=(y*(2.0*(1.0+5.0*h*h*f(r+h)/12.0))-yp*(1.0-h*h*f(r+2.0*h)/12.0))/(1.0-h*h*f(r)/12.0);
        yp=y; y=ym;
        let magnitude=y.abs().max(yp.abs());
        if magnitude>1e100 { y=y/1e100; yp=yp/1e100; for v in &mut last {*v=*v/1e100;} log_scale+=100.0*LN_10; }
        if j<5 {last[j]=y;}
    }
    let deriv=(last[0]*(-25.0)+last[1]*48.0-last[2]*36.0+last[3]*16.0-last[4]*3.0)/(12.0*h);
    let inc=(last[0]+C::new(deriv.im/kin,-deriv.re/kin))*0.5;
    let answer=((k/kin).ln()-2.0*(inc.abs().ln()+log_scale))/LN_10;
    if answer.is_finite() {Some(answer)} else {None}
}
fn result(status:u32, msg:&str)->ResultPod {
    let mut r=ResultPod {version:1,status,log10_transmission:f64::NAN,step_error_decades:f64::NAN,match_error_decades:f64::NAN,message:[0;192]};
    let b=msg.as_bytes(); let n=b.len().min(191); r.message[..n].copy_from_slice(&b[..n]); r
}
fn calculate_with_cancel(p:&Request,cancel:&dyn Fn()->bool)->ResultPod {
    if !valid(p) {return result(2,"Invalid request or inner lead is not classically allowed");}
    let (a,b,c,d)=match (solve_once(p,p.step_fm,p.match_radius_fm,cancel),solve_once(p,p.step_fm/2.0,p.match_radius_fm,cancel),solve_once(p,p.step_fm/4.0,p.match_radius_fm,cancel),solve_once(p,p.step_fm/4.0,2.0*p.match_radius_fm,cancel)) {
        (Some(a),Some(b),Some(c),Some(d))=>(a,b,c,d), _=>return if cancel() {result(4,"Cancelled")} else {result(3,"Numerical limit, invalid exterior, or Coulomb asymptotic series did not converge")}
    };
    let step=(c-b).abs().max((b-a).abs()); let matching=(c-d).abs();
    let good=step<=1e-4 && matching<=1e-4 && c<=1e-6;
    let mut r=result(if good {0} else {1},if good {"Converged"} else {"Step or exterior matching did not converge"});
    r.log10_transmission=c; r.step_error_decades=step; r.match_error_decades=matching; r
}
fn calculate(p:&Request)->ResultPod { calculate_with_cancel(p,&|| false) }
/// All non-null pointers must be aligned and valid for their respective POD structs.
#[no_mangle]
pub unsafe extern "C" fn alpha_numerov_solve(request:*const Request, output:*mut ResultPod)->i32 {
    if request.is_null() || output.is_null() {return -1}
    let r=std::panic::catch_unwind(|| calculate(&*request)).unwrap_or_else(|_|result(3,"Internal numerical failure"));
    output.write(r); 0
}
/// The callback must not unwind and its context must remain valid until return.
#[no_mangle]
pub unsafe extern "C" fn alpha_numerov_solve_cancelable(request:*const Request, output:*mut ResultPod, callback:Option<unsafe extern "C" fn(*mut std::ffi::c_void)->i32>, context:*mut std::ffi::c_void)->i32 {
    if request.is_null() || output.is_null() {return -1}
    let cancel=|| callback.map_or(false,|f| unsafe {f(context)!=0});
    let r=std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| calculate_with_cancel(&*request,&cancel))).unwrap_or_else(|_|result(3,"Internal numerical failure"));
    output.write(r); 0
}
#[cfg(test)]
mod tests {
    use super::*;
    fn fixture()->Request { Request {version:1,kind:0,l:0,reserved:0,daughter_z:0.0,reduced_mass_mev:3657.0,q_mev:5.0,depth_mev:0.0,radius_fm:7.0,diffuseness_fm:0.65,charge_radius_fm:7.0,lead_radius_fm:2.0,match_radius_fm:20.0,step_fm:0.01} }
    #[test] fn free_particle_transmission() { let p=fixture(); let r=calculate(&p); assert_eq!(r.status,0); assert!(r.log10_transmission.abs()<1e-5); }
    #[test] fn analytic_potential_step() {
        let mut p=fixture(); p.depth_mev=10.0;
        let t=solve_once(&p,0.00005,20.0,&||false).unwrap();
        let k=p.q_mev.sqrt(); let ki=(p.q_mev+p.depth_mev).sqrt();
        let exact=(4.0*k*ki/(k+ki).powi(2)).log10();
        assert!((t-exact).abs()<1e-5,"{t} versus {exact}");
    }
    #[test] fn rectangular_barrier_and_extreme_tunneling() {
        for &(height,width,h) in &[(4.0_f64,2.0_f64,0.0002_f64),(4.0,500.0,0.005)] {
            let k=1.0_f64; let kap=(height-k*k).sqrt(); let outer=width+2.0;
            let f=|r:f64| {if r>1.0+1e-10 && r<1.0+width-1e-10 {height-k*k} else if (r-1.0).abs()<1e-10 || (r-1.0-width).abs()<1e-10 {height/2.0-k*k} else {-k*k}};
            let got=propagate(0.0,outer,h,k,k,f,|r|Some(C::new(r.cos(),r.sin())),&||false).unwrap();
            let x=kap*width; let ls=if x>20.0 {x-2.0_f64.ln()} else {x.sinh().ln()}; let lt=(height*height/(4.0*k*k*kap*kap)).ln()+2.0*ls; let exact=-(if lt>40.0 {lt} else {(1.0+lt.exp()).ln()})/LN_10;
            let tolerance=if width>10.0 {0.01} else {1e-5}; assert!((got-exact).abs()<tolerance,"{got} vs {exact}"); if width>10.0 {assert!(got < -308.0);}
        }
    }

    #[test] fn independent_coulomb_reference() {
        // 80-digit mpmath F_l/G_l values generated by tests/coulomb_reference.py.
        for &(eta,l,inner,kin,outer,expected) in &[
            (0.5,0,3.0,0.7,100.0,-0.00334873661674916084),
            (5.0,1,2.0,1.0,500.0,-6.28041252505200064),
            (30.0,0,1.0,1.0,5000.0,-68.72760769330403),
        ] {
            let f=|r:f64| 2.0*eta/r+(l*(l+1)) as f64/(r*r)-1.0;
            let actual=propagate(inner,outer,0.01,1.0,kin,f,|r|outgoing(r,eta,l),&||false).unwrap();
            assert!((actual-expected).abs()<1e-5,"eta={eta}, l={l}: {actual} vs {expected}");
        }
    }
    #[test] fn invalid_inputs_and_null_pointers() {
        let mut p=fixture(); p.q_mev=-1.0; assert_eq!(calculate(&p).status,2);
        let mut out=result(0,""); unsafe { assert_eq!(alpha_numerov_solve(std::ptr::null(),&mut out),-1); assert_eq!(alpha_numerov_solve(&p,std::ptr::null_mut()),-1); assert_eq!(alpha_numerov_solve(&p,&mut out),0); } assert_eq!(out.status,2);
    }
    #[test] fn coulomb_solution_checks_exterior() {
        let mut p=fixture(); p.daughter_z=50.0; p.depth_mev=100.0; p.lead_radius_fm=5.0; p.match_radius_fm=500.0; p.step_fm=0.02;
        let r=calculate(&p); assert!(r.status<=1,"{}",String::from_utf8_lossy(&r.message)); assert!(r.match_error_decades<1e-4,"{}",r.match_error_decades);
    }
}




