// Thruster.cpp -- Implements the Propulsion Thruster types.
//
// Based on Aeromatic2 PHP code by David P. Culp
// Started June 2003
//
// C++-ified and modulized by Erik Hofman, started October 2015.
//
// Copyright (C) 2003, David P. Culp <davidculp2@comcast.net>
// Copyright (C) 2015 Erik Hofman <erik@ehofman.com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

#include <math.h>
#include <sstream>
#include <iomanip>

#include <Aircraft.h>
#include "Propulsion.h"
#include "Thruster.h"

namespace Aeromatic
{

#define FACT			2.667f
#define	NUM_PROP_PITCHES	6

Thruster::Thruster(Propulsion *p) :
    _propulsion(p)
{
}

Thruster::~Thruster()
{
    std::vector<Param*>::iterator it;
    for(it = _inputs.begin(); it != _inputs.end(); ++it) {
        delete *it;
    }
}


Direct::Direct(Propulsion *p) : Thruster(p)
{
    strCopy(_thruster_name, "direct");
}

std::string Direct::thruster()
{
    std::stringstream file;

    file << "<!--" << std::endl;
    file << "    See: http://wiki.flightgear.org/JSBSim_Thrusters#FGDirect" << std::endl;
    file << std::endl;
    file << "    Thrust is computed directly by the engine" << std::endl;
    file << "-->" << std::endl;
    file << std::endl;
    file << "<direct name=\"Direct\">" << std::endl;
    file << "</direct>" << std::endl;

    return file.str();
}


Nozzle::Nozzle(Propulsion *p) : Thruster(p),
    _diameter(3.25f)
{
    strCopy(_thruster_name, "my_nozzle");

    _inputs.push_back(new Param("Nozzle name", "The name is used for the configuration file name", _thruster_name));
    _inputs.push_back(new Param("Nozzle diameter", "Nozzle diameter influences the nozzle area and exit pressure", _diameter, _propulsion->_aircraft->_metric, LENGTH));
}

std::string Nozzle::thruster()
{
    std::stringstream file;

    float area = _diameter * _diameter * PI/4;
    float pe = area / _propulsion->_power;

    file << "<!--" << std::endl;
    file << "    See:  http://wiki.flightgear.org/JSBSim_Thrusters#FGNozzle" << std::endl;
    file << std::endl;
    file << "    pe      = Nozzle exit pressure, psf." << std::endl;
    file << "    area    = Nozzle exit area, sqft." << std::endl;
    file << "  -->" << std::endl;
    file << std::endl;
    file << "<nozzle name=\"" << _thruster_name << "\">" << std::endl;
    file << "  <pe unit=\"PSF\"> " << pe << " </pe>" << std::endl;
    file << "  <area unit=\"FT2\"> " << area << " </area>" << std::endl;
    file << "</nozzle>" << std::endl;

    return file.str();
}


Propeller::Propeller(Propulsion *p) : Thruster(p),
    _fixed_pitch(true),
    _diameter(8.0f),
    _max_rpm(2100.0f),
    _pitch_hub(48.0f),
    _pitch_tip(8.0f),
    _max_chord(0.0f),
    _pitch_levels(0)
{
    strCopy(_thruster_name, "my_propeller");

    _inputs.push_back(new Param("Thruster name", "The name is used for the configuration file name", _thruster_name));
    _inputs.push_back(new Param("Propeller diameter", "Propeller diameter is critical for a good thrust estimation", _diameter, _propulsion->_aircraft->_metric, LENGTH));
    _inputs.push_back(new Param("Is the propeller fixed pitch?", "Fixed pitch propellers do not have any mechanics to alter the pitch angle", _fixed_pitch));
}

/**
 * Option for using blade element theory:
 * http://www-mdp.eng.cam.ac.uk/web/library/enginfo/aerothermal_dvd_only/aero/propeller/prop1.html
 *
 * A simple propeller design with linear properties:
 * http://www-mdp.eng.cam.ac.uk/web/library/enginfo/aerothermal_dvd_only/aero/propeller/propel.txt
 * However with the inclusion of your own propeller geometry and section data
 * a more accurate analysis of the specific propeller design can be obtained.
 *
 * http://www.icas.org/ICAS_ARCHIVE/ICAS2010/PAPERS/434.PDF
 */
#define NUM_ELEMENTS	12
void  Propeller::bladeElement()
{
    float RPM = _engine_rpm;
    float hub = _pitch_hub;
    float tip = _pitch_tip;
    float dia = _diameter;
    float B = _blades;

    if (_max_chord == 0) {
        _max_chord = 0.17f*powf(dia,1.0f/B);
    }

    float R = dia/2.0f;
    float xt = R;
    float xs = 0.1f*R;
    float rho = 1.225f;
    float n = RPM/60.0f;
    float omega = 2.0f*PI*n;
    float coef1 = (tip-hub)/(xt-xs);
    float coef2 = hub - coef1*xs;
    float rstep = (xt-xs)/(NUM_ELEMENTS-2);

    float n2 = n*n;
    float D4 = dia*dia*dia*dia;
    float D5 = D4*dia;

    float pitch = _fixed_pitch ? 0.0f : -15.0f;
    do
    {
        float eff = 0.89f;
        float step = 0.05f;
        for (float J=0.1; J<2.4f; J += step)
        {
            if (J > 1.36f) step = 0.1f;

            float V = J*n*dia;
            float thrust = 0.0f;
            float torque = 0.0f;
            for (unsigned i=0; i<NUM_ELEMENTS-1; ++i)
            {
                float rad = xs + i*rstep;
                float r = _MAX(1.0f - rad/xt, 0.0f);
#if 0
                // historic propeller
                float chord = _max_chord*(0.25f + 0.9f*(pow(r,0.5f)-pow(r,5.0f)));
#else
                // modern propeller
                float chord = _max_chord*(0.5f + 0.53f*(pow(r,0.25f)-pow(r,5.0f)));
#endif
                float TC = 0.2f*(0.1f + 0.9f*powf(r,2.5f));

                float theta = coef1*rad + coef2+pitch;
                float th = theta*DEG_TO_RAD;
                float a = 0.1f;
                float b = 0.01f;
                int finished = 0;
                int sum = 1;

                float AR = B*rstep/chord;
                float PAR = PI*AR;

                float CL0 = 0.42f;
                float CLa = PAR/(1.0f + sqrtf(1.0f + 0.25f*AR*AR));
                float CD0 = 0.002448f*TC;
                float CDa = 2.0f*CLa/(B*eff*PAR);
                float CDi = 1.0f/(eff*PAR);

                float DtDr, DqDr, tem1, tem2, anew, bnew;
                while (finished == 0)
                {
                    float V0 = V*(1.0f+a);
                    float V2 = omega*rad*(1.0f-b);
                    float phi = atan2f(V0,V2);
                    float alpha = th-phi;

                    float CL = CL0 + CLa*alpha;
                    float CD = CD0 + CDa*alpha*CL + CDi*CL*CL;
                    float CY = CL*cosf(phi) - CD*sinf(phi);
                    float CX = CD*cosf(phi) + CL*sinf(phi);
                    float Vlocal = sqrtf(V0*V0 + V2*V2);
                    float Vlocal2 = Vlocal*Vlocal;

                    DtDr = 0.5f*rho*Vlocal2*B*chord*CY;
                    DqDr = 0.5f*rho*Vlocal2*B*chord*rad*CX;
                    tem1 = DtDr/(4.0f*PI*rad*rho*V*V*(1.0f+a));
                    tem2 = DqDr/(4.0f*PI*rad*rad*rad*rho*V*(1.0f+a)*omega);
                    anew = 0.5f*(a+tem1);
                    bnew = 0.5f*(b+tem2);
                    if (fabsf(anew-a) < 1.0e-5f && fabsf(bnew-b) < 1.0e-5f) {
                        finished=1;
                    }
                    a = anew;
                    b = bnew;
                    if (++sum > 500) {
                        finished = 1;
                    }
                }
                thrust += DtDr*rstep;
                torque += DqDr*rstep;
            }

            float CT = thrust/(rho*n2*D4);
            float CQ = torque/(rho*n2*D5);
            float CP = 2.0f*PI*CQ;

            _performance_t entry(J, CT, CP);
            _performance.push_back(entry);
        }

        _pitch_levels++;
        if (_fixed_pitch) break;
        pitch += 15.0f;
    }
    while (_pitch_levels < NUM_PROP_PITCHES);
}

void Propeller::set_thruster(float mrpm)
{
    // find rpm which gives a tip mach of 0.88 (static at sea level)
    _engine_rpm = mrpm;
    _max_rpm = 18763.0f / _diameter;
    _gear_ratio = _MAX(_engine_rpm / _max_rpm, 1.0f);

    float max_rps = _max_rpm / 60.0f;
    float rps2 = max_rps * max_rps;
    float rps3 = rps2 * max_rps;
    float d4 = _diameter * _diameter * _diameter * _diameter;
    float d5 = d4 * _diameter;
    float rho = 0.002378f;

    // power and thrust coefficients at design point
    // for fixed pitch design point is beta=22, J=0.2
    // for variable pitch design point is beta=15, j=0
    _Cp0 = _propulsion->_power * 550.0f / rho / rps3 / d5;
    if (_fixed_pitch == false)
    {
        _Ct0 = _Cp0 * 2.33f;
        _static_thrust = _Ct0 * rho * rps2 * d4;
    } else {
        float rpss = powf(_propulsion->_power * 550.0f / 1.025f / _Cp0 / rho / d5, 0.3333f);
        _Ct0 = _Cp0 * 1.4f;
        _static_thrust = 1.09f * _Ct0 * rho * rpss * rpss * d4;
    }

    // estimate the number of blades
    if (_Cp0 < 0.035f) {
      _blades = 2;
    } else if (_Cp0 > 0.160f) {
        _blades = 8;
    } else if (_Cp0 > 0.105f) {
      _blades = 6;
    } else if (_Cp0 > 0.065f) {
      _blades = 4;
    } else {
      _blades = 3;
    }

    // estimate the moment of inertia
    float weight = powf(_diameter, 2.8f) / 4.8f;
    float mass_prop = weight / 32.174f;		// Standard gravity
    float mass_hub = 0.1f * mass_prop;
    float mass_blade = (mass_prop - mass_hub) / _blades;
    float L = _diameter / 2;			// length each blade (feet)
    float R = L * 0.1f;				// radius of hub (feet) 
    float ixx_blades = _blades * (0.33333f * mass_blade * L * L);
    float ixx_hub = 0.5f * mass_hub * R * R;
    _ixx = ixx_blades + ixx_hub;

    // Thruster effects on coefficients
    Aeromatic* aircraft = _propulsion->_aircraft;
    float Swp = 0.96f*_diameter/aircraft->_wing.span;

    _dCLT0 = aircraft->_CL0*Swp;
    _dCLTmax = aircraft->_CLmax[0]*Swp;
    _dCLTalpha = aircraft->_CLaw[0]*Swp;

    _prop_span_left = 0;
    _prop_span_right = 0;
    int left = 0, right = 0;
    for (unsigned i = 0; i < aircraft->_no_engines; ++i)
    {
        if (_propulsion->_mount_point[i] == LEFT_WING)
        {
            left++;
            _prop_span_left += _propulsion->_thruster_loc[i][Y];
        }
        else if (_propulsion->_mount_point[i] == RIGHT_WING)
        {
            right++;
            _prop_span_right += _propulsion->_thruster_loc[i][Y];
        }
    }
    if (aircraft->_no_engines > 1)
    {
       _prop_span_left /= left;
       _prop_span_right /= right;
    }

    bladeElement();
}

std::string Propeller::lift()
{
    std::stringstream file;

    Aeromatic* aircraft = _propulsion->_aircraft;
    float dCL0 = _dCLT0;
    float dCLmax = _dCLTmax;
    float dCLalpha = _dCLTalpha;

    float alpha = (dCLmax-dCL0)/dCLalpha;

    file << std::setprecision(3) << std::fixed << std::showpoint;
    file << "    <function name=\"aero/force/Lift_propwash\">" << std::endl;
    file << "      <description>Delta lift due to propeller induced velocity</description>" << std::endl;
    file << "      <product>" << std::endl;
    if (aircraft->_no_engines > 1) {
        file << "         <property>systems/propulsion/thrust-coefficient</property>" << std::endl;
    } else {
        file << "         <property>propulsion/engine[0]/thrust-coefficient</property>" << std::endl;
    }
    file << "          <property>aero/qbar-psf</property>" << std::endl;
    file << "          <property>metrics/Sw-sqft</property>" << std::endl;
    file << "          <table>" << std::endl;
    file << "            <independentVar lookup=\"row\">aero/alpha-rad</independentVar>" << std::endl;
    file << "            <independentVar lookup=\"column\">fcs/flap-pos-deg</independentVar>" << std::endl;
    file << "            <tableData>" << std::endl;
    file << "                     0.0     60.0" << std::endl;
    file << "              " << std::setprecision(2) << std::setw(5) << _MIN(-dCL0/alpha, -0.01f) << "  0.000   0.000" << std::endl;
    file << "               0.00  " << std::setprecision(3) << std::setw(5) << (dCL0) << std::setw(8) << (FACT * dCL0) << std::endl;
    file << "               " << std::setprecision(2) << (alpha) << std::setprecision(3) << std::setw(7) << (dCLmax) << std::setw(8) << (FACT * dCLmax) << std::endl;
    file << "               " << std::setprecision(2) << (2.0f*alpha) << "  0.000   0.000" << std::endl;
    file << "            </tableData>" << std::endl;
    file << "          </table>" << std::endl;
    file << "      </product>" << std::endl;
    file << "    </function>" << std::endl;

    return file.str();
}

std::string Propeller::pitch()
{
    std::stringstream file;

    Aeromatic* aircraft = _propulsion->_aircraft;
    float Sw = aircraft->_wing.area;
//  float AR = aircraft->_wing.aspect;
    float lh = aircraft->_htail.arm;
    float Sh = aircraft->_htail.area;
    float cbarw = aircraft->_wing.chord_mean;
    
    float Knp = aircraft->_no_engines;
    if (Knp > 3.0f) Knp = 2.0f;
    Knp /= aircraft->_no_engines;

    float pfact = -Knp*lh*Sh/cbarw/Sw;

    float Cm0 = _dCLT0*pfact;
    float Cmmax = _dCLTmax*pfact;
    float Cmalpha = _dCLTalpha*pfact;

    float alpha = (Cmmax-Cm0)/Cmalpha;

    file << std::setprecision(3) << std::fixed << std::showpoint;
    file << "    <function name=\"aero/moment/Pitch_propwash\">" << std::endl;
    file << "      <description>Pitch moment due to propeller induced velocity</description>" << std::endl;
    file << "      <product>" << std::endl;
    if (aircraft->_no_engines > 1) {
        file << "         <property>systems/propulsion/thrust-coefficient</property>" << std::endl;
    } else {
        file << "          <property>propulsion/engine[0]/thrust-coefficient</property>" << std::endl;
    }
    file << "          <property>aero/qbar-psf</property>" << std::endl;
    file << "          <property>metrics/Sw-sqft</property>" << std::endl;
    file << "          <property>metrics/bw-ft</property>" << std::endl;
    file << "          <table>" << std::endl;
    file << "            <independentVar lookup=\"row\">aero/alpha-rad</independentVar>" << std::endl;
    file << "            <independentVar lookup=\"column\">fcs/flap-pos-deg</independentVar>" << std::endl;
    file << "            <tableData>" << std::endl;
    file << "                     0.0     60.0" << std::endl;
    file << "              " << std::setprecision(2) << std::setw(5) << _MIN(Cm0*alpha, -0.01f) << "  0.000   0.000" << std::endl;
    file << "               0.00 " << std::setprecision(3) << std::setw(6) << (Cm0) << std::setw(8) << (FACT * Cm0) << std::endl;
    file << "               " << std::setprecision(2) << (alpha) << std::setprecision(3) << std::setw(7) << (Cmmax) << std::setw(8) << (FACT * Cmmax) << std::endl;
    file << "               " << std::setprecision(2) << (1.3f*alpha) << "  0.000   0.000" << std::endl;
    file << "            </tableData>" << std::endl;
    file << "          </table>" << std::endl;
    file << "      </product>" << std::endl;
    file << "    </function>" << std::endl;

    return file.str();
}

std::string Propeller::roll()
{
    std::stringstream file;

    Aeromatic* aircraft = _propulsion->_aircraft;
    float y = _prop_span_left - _diameter/2.0f;
    float k = y/(aircraft->_wing.span/2.0f);
//  float TR = aircraft->_wing.taper;

    // http://www.princeton.edu/~stengel/MAE331Lecture5.pdf
    float dClT = (_dCLTalpha/2.0f)*((1.0f-k*k)/3.0);
    
    file << std::setprecision(4) << std::fixed << std::showpoint;
    file << "    <function name=\"aero/moment/Roll_differential_propwash\">" << std::endl;
    file << "       <description>Roll moment due to differential propwash</description>" << std::endl;
    file << "       <product>" << std::endl;
    if (aircraft->_no_engines > 1) {
        file << "           <property>systems/propulsion/thrust-coefficient-left-right</property>" << std::endl;
    } else {
        file << "           <property>propulsion/engine[0]/thrust-coefficient</property>" << std::endl;
    }
    file << "           <property>aero/qbar-psf</property>" << std::endl;
    file << "           <property>metrics/Sw-sqft</property>" << std::endl;
    file << "           <property>metrics/bw-ft</property>" << std::endl;
    file << "           <property>aero/alpha-rad</property>" << std::endl;
    file << "           <value> " << (dClT) << " </value>" << std::endl;
    file << "       </product>" << std::endl;
    file << "    </function>" << std::endl;

    return file.str();
}

std::string Propeller::thruster()
{
//  PistonEngine *engine = (PistonEngine*)_engine;
    std::stringstream file;

    file << "<!-- Generated by Aero-Matic v " << AEROMATIC_VERSION_STR << std::endl;
    file << std::endl;
    file << "    See: http://wiki.flightgear.org/JSBSim_Thrusters#FGPropeller" << std::endl;
    file << std::endl;
    file << "    Inputs:" << std::endl;
    file << "           horsepower: " << _propulsion->_power << std::endl;
    file << "                pitch: " << (_fixed_pitch ? "fixed" : "variable") << std::endl;
    file << "       max engine rpm: " << _engine_rpm << std::endl;
    file << "   prop diameter (ft): " << _diameter << std::endl;
    file << std::endl;
    file << "    Outputs:" << std::endl;
    file << "         max prop rpm: " << _max_rpm << std::endl;
    file << "           gear ratio: " << _gear_ratio << std::endl;
    file << "                  Cp0: " << _Cp0 << std::endl;
    file << "                  Ct0: " << _Ct0 << std::endl;
    file << "  static thrust (lbs): " << _static_thrust << std::endl;
    file << "-->" << std::endl;
    file << std::endl;

    file << "<propeller version=\"1.01\" name=\"prop\">" << std::endl;
    file << "  <ixx> " << _ixx << " </ixx>" << std::endl;
    file << "  <diameter unit=\"IN\"> " << (_diameter * FEET_TO_INCH) << " </diameter>" << std::endl;
    file << "  <numblades> " << _blades << " </numblades>" << std::endl;
    file << "  <gearratio> " << _gear_ratio << " </gearratio>" << std::endl;
    file << "  <cp_factor> 1.00 </cp_factor>" << std::endl;
    file << "  <ct_factor> 1.00 </ct_factor>" << std::endl;

    if(_fixed_pitch == false)
    {
        file << "  <minpitch> 12 </minpitch>" << std::endl;
        file << "  <maxpitch> 45 </maxpitch>" << std::endl;
        file << "  <minrpm> " << (_max_rpm * 0.85f) << " </minrpm>" << std::endl;
        file << "  <maxrpm> " << _max_rpm << " </maxrpm>" << std::endl;
    }
    file << std::endl;

    file << std::fixed;
    if(_fixed_pitch)
    {
        file << "  <table name=\"C_THRUST\" type=\"internal\">" << std::endl;
        file << "     <tableData>" << std::endl;
        for (unsigned i=0; i<_performance.size(); ++i)
        {
            file << std::setw(10) << std::setprecision(2) << _performance[i].J;
            file << std::setw(10) << std::setprecision(4) << _performance[i].CT;
            file << std::endl;
        }
        file << "     </tableData>" << std::endl;
        file << "  </table>" << std::endl;
        file << std::endl;
    }
    else	// variable pitch
    {
        file << " <!-- thrust coefficient as a function of advance ratio and blade angle -->" << std::endl;
        file << "  <table name=\"C_THRUST\" type=\"internal\">" << std::endl;
        file << "      <tableData>" << std::endl;
        unsigned size = _performance.size()/_pitch_levels;
        file << std::setw(16) << "";
        for (unsigned p=0; p<NUM_PROP_PITCHES; ++p) {
            file << std::setw(10) << -15+int(p*15);
        }

        file << std::endl;
        for (unsigned i=0; i<size; ++i)
        {
            file << std::setw(16) << std::setprecision(2) << _performance[i].J;
            file << std::setprecision(4);
            for (unsigned p=0; p<NUM_PROP_PITCHES; ++p) {
                file << std::setw(10) << _performance[(p*size)+i].CT;
            }
            file << std::endl;
        }
        file << "      </tableData>" << std::endl;
        file << "  </table>" << std::endl;
    }

    file << "" << std::endl;
    if(_fixed_pitch)
    {
        file << "  <table name=\"C_POWER\" type=\"internal\">" << std::endl;
        file << "     <tableData>" << std::endl;
        for (unsigned i=0; i<_performance.size(); ++i)
        {
            file << std::setw(10) << std::setprecision(2) << _performance[i].J;
            file << std::setw(10) << std::setprecision(4) << _performance[i].CP;
            file << std::endl;
        }
        file << "     </tableData>" << std::endl;
        file << "  </table>" << std::endl;
    }
    else
    {		// variable pitch
        file << " <!-- power coefficient as a function of advance ratio and blade angle -->" << std::endl;
        file << "  <table name=\"C_POWER\" type=\"internal\">" << std::endl;
        file << "     <tableData>" << std::endl;
        unsigned size = _performance.size()/_pitch_levels;
        file << std::setw(16) << "";
        for (unsigned p=0; p<NUM_PROP_PITCHES; ++p) {
            file << std::setw(10) << -15+int(p*15);
        }

        file << std::endl;
        for (unsigned i=0; i<size; ++i)
        {
            file << std::setw(16) << std::setprecision(2) << _performance[i].J;
            file << std::setprecision(4);
            for (unsigned p=0; p<NUM_PROP_PITCHES; ++p) {
                file << std::setw(10) << _performance[(p*size)+i].CP;
            }
            file << std::endl;
        }
        file << "     </tableData>" << std::endl;
        file << "  </table>" << std::endl;
    }

    file << std::endl;
    file << "<!-- thrust effects of helical tip Mach -->" << std::endl;
    file << "<table name=\"CT_MACH\" type=\"internal\">" << std::endl;
    file << "  <tableData>" << std::endl;
    file << "    0.85   1.0" << std::endl;
    file << "    1.05   0.8" << std::endl;
    file << "  </tableData>" << std::endl;
    file << "</table>" << std::endl;

    file << "" << std::endl;
    file << "<!-- power-required effects of helical tip Mach -->" << std::endl;
    file << "<table name=\"CP_MACH\" type=\"internal\">" << std::endl;
    file << "  <tableData>" << std::endl;
    file << "    0.85   1.0" << std::endl;
    file << "    1.05   1.8" << std::endl;
    file << "    2.00   1.4" << std::endl;
    file << "  </tableData>" << std::endl;
    file << "</table>" << std::endl;

    file << "\n</propeller>" << std::endl;

    return file.str();
}

// ---------------------------------------------------------------------------

#if 0
float const Propeller::_thrust_t[23][9] =
{
    {-0.488f, 0.275f, 1.000f, 1.225f, 1.350f, 1.425f, 1.313f, 1.125f, 0.0f },
    {-0.725f, 0.000f, 1.000f, 1.225f, 1.350f, 1.438f, 1.344f, 1.125f, 0.0f },
    {-0.813f,-0.250f, 0.863f, 1.200f, 1.331f, 1.438f, 1.344f, 1.125f, 0.0f },
    {-0.813f,-0.581f, 0.650f, 1.188f, 1.306f, 1.425f, 1.344f, 1.125f, 0.0f },
    {-0.813f,-0.813f, 0.344f, 1.069f, 1.250f, 1.388f, 1.325f, 1.125f, 0.0f },
    {-0.813f,-0.813f, 0.019f, 0.800f, 1.213f, 1.338f, 1.325f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.325f, 0.488f, 1.163f, 1.269f, 1.313f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.669f, 0.150f, 0.956f, 1.225f, 1.313f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.219f, 0.688f, 1.206f, 1.288f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.556f, 0.375f, 1.163f, 1.263f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f, 0.063f, 1.000f, 1.225f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.250f, 0.781f, 1.220f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.563f, 0.563f, 1.200f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f, 0.300f, 0.980f, 1.125f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f, 0.038f, 0.620f, 1.000f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.225f, 0.406f, 0.813f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.488f, 0.213f, 0.625f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.750f, 0.019f, 0.438f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.175f, 0.250f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.369f, 0.063f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.563f,-0.125f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.756f,-0.313f, 0.0f },
    {-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.813f,-0.813f, 0.0f }
};

float const Propeller::_power_t[23][9] =
{
    { 0.167f, 0.333f, 1.167f, 2.650f, 4.570f, 6.500f, 7.500f, 8.300f, 8.300f },
    { 0.667f, 0.167f, 1.000f, 2.470f, 4.370f, 6.500f, 7.530f, 8.300f, 8.300f },
    { 0.950f, 0.267f, 0.967f, 2.300f, 4.180f, 6.500f, 7.530f, 8.300f, 8.300f },
    { 1.280f, 0.583f, 0.833f, 2.120f, 3.970f, 6.500f, 7.530f, 8.300f, 8.300f },
    { 1.570f, 0.883f, 0.550f, 1.970f, 3.720f, 6.370f, 7.500f, 8.300f, 8.300f },
    { 1.850f, 1.183f, 0.167f, 1.670f, 3.500f, 6.080f, 7.500f, 8.300f, 8.300f },
    { 2.130f, 1.470f, 0.167f, 1.170f, 3.300f, 5.770f, 7.470f, 8.300f, 8.300f },
    { 2.420f, 1.175f,-0.550f, 0.450f, 2.920f, 5.530f, 7.420f, 8.300f, 8.300f },
    { 2.700f, 2.030f,-0.830f,-0.333f, 2.250f, 5.450f, 7.330f, 8.300f, 8.300f },
    { 2.980f, 2.320f,-0.970f,-1.000f, 1.420f, 5.300f, 7.170f, 8.000f, 8.300f },
    { 3.270f, 2.600f,-1.000f,-1.670f, 0.417f, 4.700f, 6.950f, 7.830f, 8.300f },
    { 3.550f, 2.880f,-1.280f,-2.330f,-0.500f, 4.000f, 6.620f, 7.670f, 8.300f },
    { 3.830f, 3.170f,-1.570f,-3.000f,-1.500f, 3.250f, 6.420f, 7.330f, 8.300f },
    { 4.120f, 3.450f,-1.850f,-3.670f,-2.500f, 2.320f, 6.230f, 7.170f, 8.300f },
    { 4.400f, 3.730f,-2.130f,-4.330f,-3.170f, 0.970f, 6.080f, 6.920f, 8.300f },
    { 4.680f, 4.020f,-2.420f,-5.000f,-3.800f,-0.330f, 5.950f, 6.830f, 8.300f },
    { 4.970f, 4.300f,-2.700f,-5.670f,-4.500f,-1.500f, 5.750f, 6.830f, 8.300f },
    { 5.250f, 4.580f,-2.980f,-6.330f,-5.170f,-2.670f, 5.380f, 6.670f, 8.300f },
    { 5.530f, 4.870f,-3.270f,-7.000f,-5.830f,-3.830f, 4.170f, 6.500f, 8.300f },
    { 5.820f, 5.150f,-3.550f,-7.670f,-6.500f,-5.000f, 2.930f, 6.330f, 8.300f },
    { 6.100f, 5.430f,-3.830f,-8.300f,-7.170f,-6.170f, 1.630f, 6.130f, 8.300f },
    { 6.380f, 5.720f,-4.120f,-8.300f,-8.300f,-7.330f, 0.330f, 5.670f, 8.300f },
    { 8.300f, 8.300f,-8.300f,-8.300f,-8.300f,-8.300f,-8.300f,-5.000f, 8.300f }
};
#endif

} /* namespace Aeromatic */
