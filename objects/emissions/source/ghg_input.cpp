/*! 
* \file ghg_input.cpp
* \ingroup CIAM
* \brief GhgInput class source file.
* \author Steve Smith
* \date $Date$
* \version $Revision$
*/

#include "util/base/include/definitions.h"
#include <string>
#include "emissions/include/ghg.h"
#include "emissions/include/ghg_input.h"

using namespace std;

//! Calculate Ghg emissions based on the input value. 
void GhgInput::calcEmission( const string& regionName, const string& fuelname, const double input, const string& prodname, const double output ) {
	
	if ( emissionsWereInput ) {
		emission = inputEmissions;
		emissFuel = inputEmissions;
		if ( input != 0 ) {
			emissCoef = inputEmissions / input;
		} else {
			emissCoef = 0;
		}
	} else {
		emission = input * emissCoef;
		emissFuel =  emission;
	}
	emissGwp = gwp * emission;
}
