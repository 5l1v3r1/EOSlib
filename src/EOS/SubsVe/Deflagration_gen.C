#include <LocalMath.h>

#include "EOS.h"
#include "Deflagration_gen.h"


Deflagration_gen::Deflagration_gen(EOS &e, double pvac)
    : Deflagration(e,pvac), ODE(1,512)
{
    rel_tol      = 1.e-6;
    ODE::epsilon = 1.e-6;
}

Deflagration_gen::~Deflagration_gen()
{
    // Null
}

int Deflagration_gen::F(double *y_prime, const double *y, double V)
{
    double e = y[0];
    double P = eos->P(V,e);
    double Gamma = eos->Gamma(V,e);
    double c2 = eos->c2(V,e);
    double dV = V0-V;
    y_prime[0] = -0.5*(P+P0 +(c2/V-Gamma*P)*dV/V)/(1.-0.5*Gamma*dV/V);
    return std::isnan(y_prime[0]);
}

class Deflagration_gen_P : public ODEfunc
{
public:
    Deflagration_gen *det;
    Deflagration_gen_P(Deflagration_gen *Det) : det(Det) {}
    double f(double V, const double *y, const double *yp);
};
double Deflagration_gen_P::f(double V, const double *y, const double *yp)
{
    double e  = y[0];
    return -det->Eos().P(V,e);
}

class Deflagration_gen_CJ : public ODEfunc
{
public:
    Deflagration_gen *det;
    Deflagration_gen_CJ(Deflagration_gen *Det) : det(Det) {}
    double f(double V, const double *y, const double *yp);
};
double Deflagration_gen_CJ::f(double V, const double *y, const double *yp)
{
    double e  = y[0];
    double P  =  det->Eos().P(V,e);
    double c2 =  det->Eos().c2(V,e);
    return V*V*(det->P0-P) - c2*(V-det->V0);
}
int Deflagration_gen::InitCJ()
{
    double Pw = eos->P(V0,e0);
    if( std::isnan(Pw) || Pw <= P0 )
        return 1;
    e[0] = e0;
    double dV = 0.01*V0;
    int status;
    if( (status=ODE::Initialize(V0,e,dV)) )
    {
        eos->ErrorHandler()->Log("Deflagration_gen::InitCJ", __FILE__, __LINE__,
                eos, "Initialize failed with status %s\n", ErrorStatus(status) );
        return 1;
    }
    Deflagration_gen_P det(this);
    double val = -P0;
    if( (status=Integrate(det, val, Vp0, e)) )
    {
        eos->ErrorHandler()->Log("Deflagration_gen::InitCJ", __FILE__, __LINE__,
                eos, "det failed with status %s\n", ErrorStatus(status) );
        return 1;
    }
    ep0 = e[0];
    Deflagration_gen_CJ CJ(this);
    val=0.0;
    if( (status=Integrate(CJ, val, Vcj, e)) )
    {
        eos->ErrorHandler()->Log("Deflagration_gen::InitCJ", __FILE__, __LINE__,
                eos, "CJ failed with status %s\n", ErrorStatus(status) );
        return 1;
    }
    ecj = e[0];
    Pcj = eos->P(Vcj,ecj);
    Dcj = V0*sqrt((P0-Pcj)/(Vcj-V0));
    ucj = (1.-Vcj/V0)*Dcj;
    return 0;
}


int Deflagration_gen::P(double p,  int dir, WaveState &wave)
{
    if( std::isnan(P0) )
        return 1;
    if( std::abs(Pcj - p) < rel_tol*Pcj  )
        return CJwave( dir, wave );
    if( std::abs(P0 - p) < rel_tol*P0  )
    {
        wave.V = Vp0;
        wave.e = ep0;
        wave.P = P0;
        wave.u = u0;
        wave.us = u0;
        return 0;
    }        
    if( p < Pcj || P0 < p )
        return 1;  
    Deflagration_gen_P det(this);
    double val=-p;
    int status = Integrate(det, val, wave.V, e);
    if( status )
    {
        eos->ErrorHandler()->Log("Deflagration_gen::P", __FILE__, __LINE__,
                eos, "det failed with status %s\n", ErrorStatus(status) );
        return 1;
    }
// set shock state  
    wave.e  = e[0];
    wave.P  = eos->P(wave.V,wave.e);
    double D = dir * V0 * sqrt( (wave.P-P0)/(V0-wave.V) );
    wave.u  = u0 + (1.-wave.V/V0)*D;
    wave.us = u0 + D;   
    return 0;
}

class Deflagration_gen_u2 : public ODEfunc
{
public:
    Deflagration_gen *det;
    Deflagration_gen_u2(Deflagration_gen *Det) : det(Det) {}
    double f(double V, const double *y, const double *yp);
};
double Deflagration_gen_u2::f(double V, const double *y, const double *yp)
{
    if( V < det->Vp0 )
    { // unphysical
      return  V - det->Vp0;
    }
    if( det->Vcj < V )
    {  // wrong branch
        return sqr(det->ucj) + V-det->Vcj;
    }
    double e  = y[0];
    double P = det->Eos().P(V,e);
    return (det->P0-P)*(V-det->V0);
}
int Deflagration_gen::u(double u1,  int dir, WaveState &wave)
{
    if( std::isnan(P0) )
        return 1;
    wave.u = u1;
    u1 = dir*(u1-u0);
    if( std::abs(ucj - u1) < -rel_tol*ucj  )
        return CJwave( dir, wave );
    if( std::abs(u1) < -rel_tol*ucj  )
    {
        wave.V = Vp0;
        wave.e = ep0;
        wave.P = P0;
        wave.u = u0;
        wave.us = u0;
        return 0;
    }        
    if( u1 < ucj || 0. < u1 )
        return 1;
    Deflagration_gen_u2 det(this);
    double val = u1*u1;
    int status = Integrate(det, val, wave.V, e);
    if( status )
    {
        eos->ErrorHandler()->Log("Deflagration_gen::u", __FILE__, __LINE__,
                eos, "det failed with status %s\n", ErrorStatus(status) );
        return 1;
    }
// set shock state  
    wave.e  = e[0];
    wave.P  = eos->P(wave.V,wave.e);
    wave.us = u0 + dir*V0/(V0-wave.V) * u1;         
    return 0;
}

class Deflagration_gen_us : public ODEfunc
{
public:
    Deflagration_gen *det;
    Deflagration_gen_us(Deflagration_gen *Det) : det(Det) {}
    double f(double V, const double *y, const double *yp);
};
double Deflagration_gen_us::f(double V, const double *y, const double *yp)
{
    if( V < det->Vp0 )
    { // unphysical
      return  V - det->Vp0;
    }
    if( det->Vcj < V )
    {  // wrong branch
        return sqr(det->Dcj/det->V0) + V-det->Vcj;
    }
    double e  = y[0];
    double P = det->Eos().P(V,e);
    return (det->P0-P)/(V-det->V0);
}
int Deflagration_gen::u_s(double us, int dir, WaveState &wave)
{
    if( std::isnan(P0) )
        return 1;
    wave.us = us;  
    us = dir*(us-u0);
    if( std::abs(Dcj - us) < rel_tol*Dcj  )
        return CJwave( dir, wave );
    if( std::abs(us) < rel_tol*Dcj  )
    {
        wave.V = Vp0;
        wave.e = ep0;
        wave.P = P0;
        wave.u = u0;
        wave.us = u0;
        return 0;
    }        
    if( us < 0. || Dcj < us )
        return 1;
    Deflagration_gen_us det(this);
    double val = sqr(us/V0);
    int status = Integrate(det, val, wave.V, e);
    if( status )
    {
        eos->ErrorHandler()->Log("Deflagration_gen::us", __FILE__, __LINE__,
                eos, "det failed with status %s\n", ErrorStatus(status) );
        return 1;
    }
    wave.e  = e[0];
    wave.P  = eos->P(wave.V,wave.e);
    wave.u  = u0 + dir*(1. - wave.V/V0)*us;
    return 0;
}

int Deflagration_gen::V(double v1,  int dir, WaveState &wave)
{
    if( std::isnan(P0) )
        return 1;
    if( std::abs(Vcj - v1) < rel_tol*Vcj )
        return CJwave( dir, wave );
    if( std::abs(Vp0-v1) < rel_tol*Vp0  )
    {
        wave.V = Vp0;
        wave.e = ep0;
        wave.P = P0;
        wave.u = u0;
        wave.us = u0;
        return 0;
    }        
    if( v1 < Vp0 || Vcj < v1 )
        return 1;
    int status = Integrate(wave.V, e);
    if( status )
    {
        eos->ErrorHandler()->Log("Deflagration_gen::V", __FILE__, __LINE__,
                eos, "det failed with status %s\n", ErrorStatus(status) );
        return 1;
    }
    wave.e  = e[0];
    wave.P  = eos->P(wave.V,wave.e);
    double dV = V0-wave.V;
    double D = dir * V0 * sqrt( (wave.P-P0)/dV );
    wave.us = u0 + D;   
    wave.u  = u0 + (dV/V0)*D;
    return 0;
}
