#include "alpha/deformation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace alpha {
namespace {
constexpr double pi=3.14159265358979323846, e2=1.43996448;
double raw(Deformation d,double x) { double x2=x*x; return 1+d.beta2*std::sqrt(5/(16*pi))*(3*x2-1)+d.beta4*(3/(16*std::sqrt(pi)))*(35*x2*x2-30*x2+3); }
void validate(Deformation d) {
 if (!std::isfinite(d.beta2)||!std::isfinite(d.beta4)||std::abs(d.beta2)>0.5||std::abs(d.beta4)>0.5) throw std::invalid_argument("Deformation must be finite with |beta2|, |beta4| <= 0.5");
 // The shape is a quadratic in x^2, so endpoints and its interior vertex suffice.
 double a=d.beta4*3/(16*std::sqrt(pi))*35, b=d.beta2*std::sqrt(5/(16*pi))*3-d.beta4*3/(16*std::sqrt(pi))*30;
 double m=std::min(raw(d,0),raw(d,1)); if(a!=0) {double y=-b/(2*a); if(y>0&&y<1) m=std::min(m,raw(d,std::sqrt(y)));} if(m<=0) throw std::invalid_argument("Shape radius must be positive");
}
double normalization(Deformation d) { double sum=0; constexpr int n=128; for(int i=0;i<=n;i++){double v=raw(d,double(i)/n); sum+=(i==0||i==n?1:i%2?4:2)*v*v*v;} return std::cbrt(sum/(3*n)); }
double legendre(int l,double x) { if(l==0)return 1; double p0=1,p1=x; for(int k=2;k<=l;k++){double p=((2*k-1)*x*p1-(k-1)*p0)/k;p0=p1;p1=p;} return p1; }
}
double deformed_radius(double r,Deformation d,double x) { validate(d); if(!std::isfinite(r)||r<=0||!std::isfinite(x)||std::abs(x)>1) throw std::invalid_argument("Invalid radius or angle"); return r*raw(d,x)/normalization(d); }
double deformed_coulomb(const SphericalPotential& p,Deformation d,double r,double x,int order,int n) {
 validate(d); if(!std::isfinite(r)||r<=0||!std::isfinite(x)||std::abs(x)>1||order<0||order>32||n<16||n>1024) throw std::invalid_argument("Invalid Coulomb quadrature");
 if(d.beta2==0&&d.beta4==0) return coulomb_potential(p,r);
 const double R=p.charge_radius_fm, norm=normalization(d); double sum=0;
 // Uniform charge in the actual volume; radial source integration is analytic.
 for(int l=0;l<=order;l+=2){ double integ=0; for(int i=0;i<n;i++){ double u=(i+0.5)/n, a=R*raw(d,u)/norm, z=std::min(a,r); double radial=std::pow(z/r,l+1)*z*z/(l+3); if(a>r) { if(l==2) radial+=r*r*std::log(a/r); else radial+=r*r*(std::pow(a/r,2-l)-1)/(2-l); } integ+=legendre(l,u)*radial; } sum+=legendre(l,x)*integ/n; }
 return 3*2*p.daughter_z*e2*sum/(R*R*R);
}
}
namespace alpha {
DeformedWkbResult calculate_deformed_wkb(const SphericalWkbRequest& req, Deformation d) {
 DeformedWkbResult out;
 try {
  validate(d);
  if(d.beta2==0&&d.beta4==0) { auto s=calculate_spherical_wkb(req); out.status=s.status; out.message=s.message; out.log10_penetrability=s.log10_penetrability; out.log10_half_life_s=s.log10_half_life_s; out.angular_points=1; return out; }
  // Compare independent multipole truncations at several physically relevant points.
  for(double x: {0.0,0.5,1.0}) for(double r: {req.potential.charge_radius_fm,1.5*req.potential.charge_radius_fm,3*req.potential.charge_radius_fm}) out.coulomb_error_mev=std::max(out.coulomb_error_mev,std::abs(deformed_coulomb(req.potential,d,r,x,24,256)-deformed_coulomb(req.potential,d,r,x,32,512)));
  out.multipole_order=24;
  auto average=[&](int n) {
   double maxlog=-INFINITY, sum=0;
   for(int i=0;i<n;i++) {
    // Gauss-Legendre nodes on [0,1].
    double z=std::cos(pi*(i+0.75)/(n+0.5)), last, deriv=0;
    do { double p0=1,p1=z; for(int k=2;k<=n;k++){double p=((2*k-1)*z*p1-(k-1)*p0)/k;p0=p1;p1=p;} deriv=n*(z*p1-p0)/(z*z-1); last=z; z=last-p1/deriv; } while(std::abs(z-last)>1e-14);
    double x=(z+1)/2, weight=1/((1-z*z)*deriv*deriv);
    auto p=req.potential; p.nuclear_radius_fm=deformed_radius(p.nuclear_radius_fm,d,x);
    auto f=[&](double r){return nuclear_potential(p,r)+deformed_coulomb(p,d,r,x,24,256)+centrifugal_potential(p,r);};
    auto result=calculate_radial_wkb(req,f,p.nuclear_radius_fm);
    if(!result.log10_penetrability) throw std::runtime_error("An orientation has no supported barrier: "+result.status);
    double v=*result.log10_penetrability+std::log10(weight);
    if(v>maxlog){sum=std::isfinite(maxlog)?sum*std::pow(10.0,maxlog-v)+1:1; maxlog=v;} else sum+=std::pow(10.0,v-maxlog);
   }
   return maxlog+std::log10(sum);
  };
  double prior=average(8), value=prior;
  for(int n: {16,32,64}) { value=average(n); out.angular_points=n; out.angular_error_log10=std::abs(value-prior); if(out.angular_error_log10<1e-4) break; prior=value; }
  if(out.angular_error_log10>=1e-4 || out.coulomb_error_mev>0.01) {out.status="numerical_error"; out.message="Angular or Coulomb multipole convergence threshold not met."; return out;}
  out.log10_penetrability=value; out.status=req.preformation==0?"zero_preformation":"ok";
  if(req.preformation>0) out.log10_half_life_s=std::log10(std::log(2.0))-std::log10(req.assault_hz)-std::log10(req.preformation)-value;
  return out;
 } catch(const std::exception& e) { out.status="invalid_or_numerical_error"; out.message=e.what(); return out; }
}
}
