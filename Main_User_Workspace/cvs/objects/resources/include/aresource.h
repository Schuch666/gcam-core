/*
 * LEGAL NOTICE
 * This computer software was prepared by Battelle Memorial Institute,
 * hereinafter the Contractor, under Contract No. DE-AC05-76RL0 1830
 * with the Department of Energy (DOE). NEITHER THE GOVERNMENT NOR THE
 * CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY
 * LIABILITY FOR THE USE OF THIS SOFTWARE. This notice including this
 * sentence must appear on any copies of this computer software.
 * 
 * EXPORT CONTROL
 * User agrees that the Software will not be shipped, transferred or
 * exported into any country or used in any manner prohibited by the
 * United States Export Administration Act or any other applicable
 * export laws, restrictions or regulations (collectively the "Export Laws").
 * Export of the Software may require some form of license or other
 * authority from the U.S. Government, and failure to obtain such
 * export control license may result in criminal liability under
 * U.S. laws. In addition, if the Software is identified as export controlled
 * items under the Export Laws, User represents and warrants that User
 * is not a citizen, or otherwise located within, an embargoed nation
 * (including without limitation Iran, Syria, Sudan, Cuba, and North Korea)
 *     and that User is not otherwise prohibited
 * under the Export Laws from receiving the Software.
 * 
 * All rights to use the Software are granted on condition that such
 * rights are forfeited if User fails to comply with the terms of
 * this Agreement.
 * 
 * User agrees to identify, defend and hold harmless BATTELLE,
 * its officers, agents and employees from all liability involving
 * the violation of such Export Laws, either directly or indirectly,
 * by User.
 */

#ifndef _ARESOURCE_H_
#define _ARESOURCE_H_
#if defined(_MSC_VER)
#pragma once
#endif

/*!
 * \file aresource.h
 * \ingroup Objects
 * \brief AResource header file.
 * \author Josh Lurz, Sonny Kim
 */
#include <xercesc/dom/DOMNode.hpp>
#include "util/base/include/ivisitable.h"

class Tabs;
class GDP;
class IInfo;
class IOutput;
class AGHG;

/*! 
* \ingroup Objects
* \brief An abstract class which defines a single resource.
* \todo This class needs much more documentation.
* \author Josh Lurz
*/
class AResource: public IVisitable {
    friend class XMLDBOutputter;
public:
    virtual ~AResource();

    virtual void XMLParse( const xercesc::DOMNode* aNode ) = 0;

    virtual void toInputXML( std::ostream& aOut,
                             Tabs* aTabs ) const = 0;

    virtual void toDebugXML( const int aPeriod,
                             std::ostream& aOut,
                             Tabs* aTabs ) const = 0;

    virtual const std::string& getName() const = 0;

    virtual void completeInit( const std::string& aRegionName,
                               const IInfo* aRegionInfo ) = 0;
    
    virtual void initCalc( const std::string& aRegionName,
                           const int aPeriod ) = 0;

    virtual void postCalc( const std::string& aRegionName,
                           const int aPeriod ) = 0;
    
    virtual void calcSupply( const std::string& aRegionName,
                             const GDP* aGDP,
                             const int period ) = 0;

    virtual double getAnnualProd( const std::string& aRegionName,
                                  const int aPeriod ) const = 0;

    virtual void dbOutput( const std::string& aRegionName ) = 0;

    virtual void csvOutputFile( const std::string& aRegionName ) = 0;

    virtual void accept( IVisitor* aVisitor,
                         const int aPeriod ) const = 0;
protected:
    // For the database output
    virtual const std::string& getXMLName() const = 0;
    //! Resource name.
    std::string mName;
    //! Unit of resource output
    std::string mOutputUnit; 
    //! Unit of resource price
    std::string mPriceUnit; 
    //! Market name.
    std::string mMarket;
    //! A map of a keyword to its keyword group
    std::map<std::string, std::string> mKeywordMap;
    //! Vector of output objects representing the outputs of the technology.
    std::vector<IOutput*> mOutputs;
    std::vector<AGHG*> ghg;//! Suite of greenhouse gases
};

// Inline function definitions
inline AResource::~AResource(){
}

#endif // _ARESOURCE_H_
