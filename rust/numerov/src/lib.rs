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
fn solve_once(p:&Request, step:f64, outer:f64)->Option<f64> {
    let k=(2.0*p.reduced_mass_mev*p.q_mev).sqrt()/HBAR;
    let eta=p.reduced_mass_mev*(2.0*p.daughter_z*E2)/(HBAR*HBAR*k);
    let n=((outer-p.lead_radius_fm)/step).ceil() as usize;
    if n<4 || n>5_000_000 {return None}
    let h=(outer-p.lead_radius_fm)/(n as f64);
    let f=|r:f64| 2.0*p.reduced_mass_mev*(potential(p,r)-p.q_mev)/(HBAR*HBAR);
    // At matching radius the residual nuclear tail must be negligible.
    if p.kind==1 && p.depth_mev/(1.0+((outer-p.radius_fm)/p.diffuseness_fm).exp())>1e-12*p.q_mev {return None}
    if outer<=p.charge_radius_fm.max(p.radius_fm) {return None}
    let mut yp=outgoing(k*outer,eta,p.l)?; let mut y=outgoing(k*(outer-h),eta,p.l)?;
    let mut log_scale=0.0; let mut last=[C::default();5];
    for j in (0..=n-2).rev() {
        let r=p.lead_radius_fm+(j as f64)*h;
        let ym=(y*(2.0*(1.0+5.0*h*h*f(r+h)/12.0))-yp*(1.0-h*h*f(r+2.0*h)/12.0))/(1.0-h*h*f(r)/12.0);
        yp=y; y=ym;
        let magnitude=y.abs().max(yp.abs());
        if magnitude>1e100 { y=y/1e100; yp=yp/1e100; log_scale+=100.0*LN_10; }
        if j<5 {last[j]=y;}
    }
    let deriv=(last[0]*(-25.0)+last[1]*48.0-last[2]*36.0+last[3]*16.0-last[4]*3.0)/(12.0*h);
    let kin=(2.0*p.reduced_mass_mev*(p.q_mev-potential(p,p.lead_radius_fm))).sqrt()/HBAR;
    let inc=(last[0]+C::new(deriv.im/kin,-deriv.re/kin))*0.5;
    let answer=((k/kin).ln()-2.0*(inc.abs().ln()+log_scale))/LN_10;
    if answer.is_finite() {Some(answer)} else {None}
}
