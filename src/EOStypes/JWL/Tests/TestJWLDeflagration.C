#include <Arg.h>
#include <LocalMath.h>
#include "EOS.h"
#include "JWL.h"
//
#define NaN EOS::NaN

using namespace std;

int HugFail(double P0, HydroState &state0, WaveState &shock1)
{
    double us = shock1.us;
    double P1 = shock1.P;
    double m1  = (us-shock1.u)/shock1.V;
    double m0  = (us-state0.u)/state0.V;
    if( std::abs(m1-m0) > 0.001*std::abs(m1) + 1.e-6 )
        return 1;
    double p0  = P0 + m0*m0*state0.V;
    double p1  = P1 + m1*m1*shock1.V;
    if( std::abs(p1-p0) > 0.001*std::abs(p1) + 1.e-6 )
        return 1;
    double h0 = 0.5*sqr(m0*state0.V) + state0.e + P0*state0.V;
    double h1 = 0.5*sqr(m1*shock1.V) + shock1.e + P1*shock1.V;
    if( std::abs(h1-h0) > 0.001*std::abs(h1) + 1.e-6 )
        return 1;
    return 0;    
}

int diff(WaveState &shock1, WaveState &shock2)
{
    if( std::abs(shock1.V - shock2.V) > 0.001*shock1.V + 1e-6)
        return 1;
    if( std::abs(shock1.e - shock2.e) > 0.001*std::abs(shock1.e) + 1e-6)
        return 1;
    if( std::abs(shock1.u - shock2.u) > 0.001*std::abs(shock1.u) + 1e-6 )
        return 1;
    if( std::abs(shock1.P - shock2.P) > 0.001*shock1.P + 1e-6)
        return 1;
    if( std::abs(shock1.us - shock2.us) > 0.001*std::abs(shock1.us) + 1e-6)
        return 1;
    return 0;    
}

int main(int, char **argv)
{
    ProgName(*argv);
	std::string file_;
	file_ = getenv("EOSLIB_DATA_PATH");
	file_ += "/test_data/JWLTest.data";
	const char *file = file_.c_str();
	const char *type = "JWL";
	const char *name = "HMX";
	const char *units = NULL;
    double P0    = 0.1;
    int nsteps = 10;

    while(*++argv)
    {
		GetVar(file,file);
		GetVar(files,file);
		GetVar(name,name);
		GetVar(units,units);
        
        GetVar(P0,P0);
        GetVar(nsteps,nsteps);
        
        ArgError;
    }
	if( file == NULL )
		cerr << Error("Must specify data file") << Exit;
	DataBase db;
	if( db.Read(file) )
		cerr << Error("Read failed" ) << Exit;
	EOS *eos = FetchEOS(type,name,db);
	if( eos == NULL )
	{
		cerr << "eos == NULL\n";
		cerr << Error("Fetch failed") << Exit;
	}
    if( units && eos->ConvertUnits(units, db) )
		cerr << Error("ConvertUnits failed") << Exit;
    double Vref = eos->V_ref;
    double eref = eos->e_ref;
    HydroState state0(Vref,eref);
    //
    cout << "\nCJ deflagration state\n";
    Deflagration *det = eos->deflagration(state0,P0);
    if( det == NULL )
        cerr << Error("eos->deflagration failed" ) << Exit;
    WaveState CJ;
    if( det->CJwave(RIGHT,CJ) )
        cerr << Error("det->CJwave failed" ) << Exit;
    double T = eos->T(CJ.V,CJ.e);
    cout << "V e P T = " << CJ.V << ", " << CJ.e 
                 << ", " << CJ.P << ", " << T << "\n";
    cout << "D, u, c = " << CJ.us << ", " << CJ.u
                 << ", " << eos->c(CJ.V,CJ.e) << "\n";
    if( HugFail(P0,state0,CJ) )
        cerr << Error("Hugoniot not satisfied for CJ state") << Exit;
    //
    Deflagration *det_gen = eos->EOS::deflagration(state0,P0);
    if( det_gen == NULL )
        cerr << Error("det_gen is NULL" ) << Exit;
    WaveState CJ_gen;
    if( det_gen->CJwave(RIGHT,CJ_gen) )
        cerr << Error("det_gen->CJwave failed" ) << Exit;
    if( diff(CJ,CJ_gen) )
            cout << "\t det_gen->CJwave differs\n";
    //
    int i;
    WaveStateLabel(cout << "\nWeak deflagration locus\n")
        << " " <<setw(6) << "T " << "\n";
    for( i=0; i<=nsteps; i++ )
    {
        double u = double(i)/double(nsteps)*CJ.u;
        WaveState shock,shock2;
        if( det->u(u,RIGHT,shock) )
            cerr << Error("det->u failed" ) << Exit;
        T = eos->T(shock.V,shock.e);
	    cout.setf(ios::scientific, ios::floatfield);
        cout << shock;
	    cout.setf(ios::fixed, ios::floatfield);
        cout << " " << setw(6) << setprecision(0) << T
             << "\n";
        if( HugFail(P0,state0,shock) )
            cout << "\t Hugoniot not satisfied\n";
        //
        if( det_gen->u(u,RIGHT,shock2) )
            cerr << Error("det_gen->u failed" ) << Exit;
        if( diff(shock,shock2) )
            cout << "\t det_gen->u differs\n";
        //
        if( det->P(shock.P,RIGHT,shock2) )
            cerr << Error("det->P failed" ) << Exit;
        if( diff(shock,shock2) )
        {
	        cout.setf(ios::scientific, ios::floatfield);
            cout << shock2 << "\n";
            cout << "\t det->P differs\n";
        }
        if( det_gen->P(shock.P,RIGHT,shock2) )
            cerr << Error("det_gen->P failed" ) << Exit;
        if( diff(shock,shock2) )
            cout << "\t det_gen->P differs\n";
        //
        if( det->u_s(shock.us,RIGHT,shock2) )
            cerr << Error("det->u_s failed" ) << Exit;
        if( diff(shock,shock2) )
            cout << "\t det->u_s differs\n";
        if( det_gen->u_s(shock.us,RIGHT,shock2) )
            cerr << Error("det_gen->u_s failed" ) << Exit;
        if( diff(shock,shock2) )
        {
	        cout.setf(ios::scientific, ios::floatfield);
            cout << shock2 << "\n";   
            cout << "\t det_gen->u_s differs\n";
        }
        //
        if( det->V(shock.V,RIGHT,shock2) )
            cerr << Error("det->V failed" ) << Exit;
        if( diff(shock,shock2) )
            cout << "\t det->V differs\n";        
        if( det_gen->V(shock.V,RIGHT,shock2) )
            cerr << Error("det_gen->V failed" ) << Exit;
        if( diff(shock,shock2) )
            cout << "\t det_gen->V differs\n";        
    }
    delete det_gen;
    delete det;
    deleteEOS(eos);
    return 0;
}    
