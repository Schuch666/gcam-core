/*!
 * \file tree_land_allocator.cpp
 * \ingroup Objects
 * \brief TreeLandAllocator class source file.
 * \author James Blackwood
 */

#include "util/base/include/definitions.h"
#include "util/base/include/xml_helper.h"

#include "land_allocator/include/tree_land_allocator.h"
#include "containers/include/scenario.h"
#include "containers/include/iinfo.h"
#include "util/base/include/model_time.h"
#include "ccarbon_model/include/carbon_model_utils.h"
#include "util/base/include/configuration.h"
#include "ccarbon_model/include/luc_carbon_summer.h"

using namespace std;
using namespace xercesc;

extern Scenario* scenario;

/*!
 * \brief Constructor.
 * \author James Blackwood
\todo Change initialization of mCalculated so that is independent of when carbon model is initialized
 */
TreeLandAllocator::TreeLandAllocator()
: LandNode( 0 ),
  mCalDataExists( false ), 
  mCalculated( CarbonModelUtils::getStartYear(), CarbonModelUtils::getEndYear() )
{
}

//! Destructor
TreeLandAllocator::~TreeLandAllocator() {
}

const string& TreeLandAllocator::getXMLName() const {
    return getXMLNameStatic();
}

/*! \brief Get the XML node name in static form for comparison when parsing XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* The "==" operator that is used when parsing, required this second function to return static.
* \note A function cannot be static and virtual.
* \author James Blackwood
* \return The constant XML_NAME as a static.
*/
const string& TreeLandAllocator::getXMLNameStatic() {
    const static string XML_NAME = "LandAllocatorRoot";    // original XML tag
    return XML_NAME;
}

bool TreeLandAllocator::XMLParse( const DOMNode* aNode ){
    // Call the XML parse.
    return LandNode::XMLParse( aNode );
}

bool TreeLandAllocator::XMLDerivedClassParse( const string& aNodeName, const DOMNode* aCurr ){
    if( aNodeName == "landAllocation" ){
        XMLHelper<Value>::insertValueIntoVector( aCurr, mLandAllocation, scenario->getModeltime() );
    }
    else {
        return false;
    }
    return true;
}

void TreeLandAllocator::toDebugXML( const int aPeriod, std::ostream& aOut, Tabs* aTabs ) const {
    // Call the node toDebugXML
    ALandAllocatorItem::toDebugXML( aPeriod, aOut, aTabs );
}

void TreeLandAllocator::toInputXML( std::ostream& aOut, Tabs* aTabs ) const {
    // Call the node toInputXML
    LandNode::toInputXML( aOut, aTabs );
}

void TreeLandAllocator::toInputXMLDerived( ostream& aOut, Tabs* aTabs ) const {
    XMLWriteVector( mLandAllocation, "landAllocation", aOut, aTabs, scenario->getModeltime() );
}

void TreeLandAllocator::initCalc( const string& aRegionName, const int aPeriod )
{
    resetToCalibrationData( aRegionName, aPeriod );
}

void TreeLandAllocator::completeInit( const string& aRegionName, 
                                      const IInfo* aRegionInfo )
{
    checkRotationPeriod( aRegionInfo );

    // Call generic node method (since TreeLandAllocator is just a specialized node)
     LandNode::completeInit( aRegionName, aRegionInfo );

    const Modeltime* modeltime = scenario->getModeltime();
    for( int period = 0; period < modeltime->getmaxper(); period++ ) {
        resetToCalibrationData( aRegionName, period );
    }
}

/*!
 * \brief Initialize with calibrated data.
 * \details Re-allocates unmananged land if necessary, sets initital shares based on 
 *          calibrated data, and sets intrinsicYieldMode based on calibrated data
 * \param aRegionInfo Region info.
 * \param aPeriod model period.
 */
void TreeLandAllocator::resetToCalibrationData( const string& aRegionName, const int aPeriod ) {

   if ( mCalDataExists[ aPeriod ] ) {
      resetToCalLandAllocation( aPeriod );

      adjustTotalLand( aPeriod );

      // Don't do this in 1975 since values may not be correct
      if ( aPeriod > 0 ) {
         // Now re-allocate unmanaged land. Read-in land allocations are used as
         // weights with total unmanaged land set to be equal to total land minus
         // land allocation.
         double unmanagedLand = mLandAllocation[ aPeriod ]
                               - getTotalLandAllocation( eManaged, aPeriod );
         
         // Log change in unmanaged land
         double previousUnmanagedLand = getTotalLandAllocation( eUnmanaged, aPeriod );
         if ( abs( previousUnmanagedLand - unmanagedLand) > 0.01 * unmanagedLand ) {
            ILogger& mainLog = ILogger::getLogger( "main_log" );
            mainLog.setLevel( ILogger::WARNING );
            mainLog << "Unmanaged land in region " << aRegionName << " was reduced by " 
                    << previousUnmanagedLand - unmanagedLand << " ("
                    << ( previousUnmanagedLand - unmanagedLand ) / unmanagedLand << "%) "
                    << " in period " << aPeriod
                    << endl;
         }
         
         LandNode::setUnmanagedLandAllocation( aRegionName, unmanagedLand, aPeriod );

          // Check that the final allocations sum correctly.
          assert( util::isEqual( getTotalLandAllocation( eAnyLand, aPeriod ),
                                mLandAllocation[ aPeriod ].get(),
                                util::getSmallNumber() ) );
          assert( util::isEqual( getTotalLandAllocation( eUnmanaged, aPeriod ),
                                unmanagedLand,
                                util::getSmallNumber() ) );
          assert( util::isEqual( getTotalLandAllocation( eManaged, aPeriod ),
                                mLandAllocation[ aPeriod ] - unmanagedLand,
                                util::getSmallNumber() ) );
      }

      setInitShares( aRegionName,
                    0, // No parent sigma
                    0, // No land allocation above this node.
                    1, // Share of land use history is 100%.
                    mLandUseHistory.get(),
                    aPeriod );

      setIntrinsicYieldMode( 1, // Intrinsic yield mode is one for the root.
                            mSigma,
                            aPeriod );
   }
}

/*!
 * \brief Check whether the rotation period is valid and the time steps are
 *        equal.
 * \details Checks whether the model periods are all equal and evenly divide
 *          into the number of rotation years. Prints a warning if either
 *          condition is not met.
 * \param aRegionInfo Region info.
 */
void TreeLandAllocator::checkRotationPeriod( const IInfo* aRegionInfo ) const {
    // Check that all model periods are equal, which is required for this land
    // allocator.
    const Modeltime* modeltime = scenario->getModeltime();
    for( int period = 1; period < modeltime->getmaxper(); ++period ) {
        if ( modeltime->gettimestep( period ) != modeltime->gettimestep( period - 1 ) ) {
            ILogger& mainLog = ILogger::getLogger( "main_log" );
            mainLog.setLevel( ILogger::WARNING );
            mainLog << "All time steps are not constant." << endl;
        }
    }

    // A check to verify that the rotation period is a multiple of the model's
    // time step.
    const int rotationPeriod = aRegionInfo->getInteger( "rotationPeriod", true );
    for( int period = 0; period < modeltime->getmaxper(); ++period ) {
        if( rotationPeriod % modeltime->gettimestep( period ) != 0 ){
            ILogger& mainLog = ILogger::getLogger( "main_log" );
            mainLog.setLevel( ILogger::DEBUG );
            mainLog << "Rotation period is not evenly divisible by time step in period "
                    << period << "." << endl;
        }
    }
}

/*!
 * \brief Adjust the amount of total land if the calibrated production land
 *        exceeds the total.
 * \details Determines the total land allocated to production leaves and
 *          increases the total land by 20% if the total calibrated land exceeds
 *          the total.
 * \param aPeriod Model period.
 * \todo Is it worth adjusting the total land instead of just warning the user?
 */
void TreeLandAllocator::adjustTotalLand( const int aPeriod ){

    const double totalManagedLand = getTotalLandAllocation( eManaged, aPeriod );

    // Check that the total calLandUsed is not greater than the total available.
    if ( totalManagedLand - mLandAllocation[ aPeriod ] > util::getSmallNumber() ) {
        ILogger& mainLog = ILogger::getLogger( "main_log" );
        mainLog.setLevel( ILogger::DEBUG );
        mainLog << "The total managed land allocated is greater than the total land value of "
                << mLandAllocation[ aPeriod ] 
                << " in year " << scenario->getModeltime()->getper_to_yr( aPeriod ) << " by "
                << totalManagedLand - mLandAllocation[ aPeriod ] 
                << "(" << 100 * ( totalManagedLand - mLandAllocation[ aPeriod ] ) / mLandAllocation[ aPeriod ]
                << "%)" << endl;


        mainLog.setLevel( ILogger::WARNING );
        mainLog << "Total land value set to total managed land plus 20% " << endl;

        mLandAllocation[ aPeriod ] = totalManagedLand * 1.2;
    }
}

void TreeLandAllocator::addLandUsage( const string& aLandType,
                                      const string& aProductName,
                                      const LandUsageType aLandUsageType,
                                      const int aPeriod )
{
    // Find the parent land item which should have a leaf added.
    ALandAllocatorItem* parent = findChild( aLandType, eNode );

    // Check that the parent exists.
    if( !parent ){
        ILogger& mainLog = ILogger::getLogger( "main_log" );
        mainLog.setLevel( ILogger::WARNING );
        mainLog << "Cannot add a land usage for " << aProductName << " as the land type "
                << aLandType << " does not exist." << endl;
        return;
    }
    parent->addLandUsage( aLandType, aProductName, aLandUsageType, aPeriod );
}

double TreeLandAllocator::getLandAllocation( const string& aLandType,
                                             const string& aProductName,
                                             const int aPeriod ) const
{
    const ALandAllocatorItem* node = findChild( aLandType, eNode );

    if( node ){
        return node->getLandAllocation( aLandType, aProductName, aPeriod );
    }
    return 0;
}

void TreeLandAllocator::applyAgProdChange( const string& aLandType,
                                           const string& aProductName,
                                           const double aAgProdChange,
                                           const int aHarvestPeriod, 
                                           const int aCurrentPeriod )
{
    // Search for the correct land node.
    ALandAllocatorItem* node = findChild( aLandType, eNode );
    if( node ){
        node->applyAgProdChange( aLandType, aProductName,
                                         aAgProdChange, aHarvestPeriod, aCurrentPeriod );
    }
}

void TreeLandAllocator::calcYield( const string& aLandType,
                                   const string& aProductName,
                                   const string& aRegionName,
                                   const double aProfitRate,
                                   const int aHarvestPeriod,
                                   const int aCurrentPeriod )
{
    // Locate the subtree containing the land type. This is required to determine the
    // average intrinsic rate. 
    const ALandAllocatorItem* subTreeRoot = findParentOfType( aLandType );

    // This can only happen if there is an error in the input.
    if( !subTreeRoot ){
        return;
    }

    // The subtree will be this object if the sigma is greater than zero.
    assert( subTreeRoot == this || util::isEqual( mSigma.get(), 0.0 ) );

    // Find the correct land type.
    ALandAllocatorItem* node = findChild( aLandType, eNode );

    if( node ){
        node->calcYieldInternal( aLandType, aProductName,
                                 aRegionName, aProfitRate,
                                 subTreeRoot->getInstrinsicRate( aCurrentPeriod ),
                                 aHarvestPeriod, aCurrentPeriod );
    }
}

double TreeLandAllocator::getYield( const string& aLandType,
                                    const string& aProductName,
                                    const int aPeriod ) const
{
    const ALandAllocatorItem* node = findChild( aLandType, eNode );
    if( node ){
        return node->getYield( aLandType, aProductName, aPeriod );
    }
    return 0;
}

void TreeLandAllocator::setCalLandAllocation( const string& aLandType,
                                              const string& aProductName,
                                              const double aCalLandUsed,
                                              const int aHarvestPeriod, 
                                              const int aCurrentPeriod )
{
    mCalDataExists[ aCurrentPeriod ] = true;
    // TODO: This is called before the completeInit of the LandAllocator.
    ALandAllocatorItem* node = findChild( aLandType, eNode );
    if( node ){
        node->setCalLandAllocation( aLandType, aProductName, aCalLandUsed,
                                    aHarvestPeriod, aCurrentPeriod );
    }
}

void TreeLandAllocator::setCalObservedYield( const string& aLandType,
                                             const string& aProductName,
                                             const double aCalObservedYield,
                                             const int aPeriod )
{
    // TODO: This is called before the completeInit of the LandAllocator.
    ALandAllocatorItem* node = findChild( aLandType, eNode );
    if( node ){
        node->setCalObservedYield( aLandType, aProductName,
                                   aCalObservedYield, aPeriod );
    }
}

void TreeLandAllocator::setIntrinsicRate( const string& aRegionName,
                                          const string& aLandType,
                                          const string& aProductName,
                                          const double aIntrinsicRate,
                                          const int aPeriod )
{
    ALandAllocatorItem* node = findChild( aLandType, eNode );
    if( node ){
        node->setIntrinsicRate( aRegionName, aLandType, aProductName,
                                aIntrinsicRate, aPeriod );
    }
}

void TreeLandAllocator::setInitShares( const string& aRegionName,
                                       const double aSigmaAbove,
                                       const double aLandAllocationAbove,
                                       const double aParentHistoryShare,
                                       const LandUseHistory* aParentHistory,
                                       const int aPeriod )
{
    // First adjust value of unmanaged land nodes
    setUnmanagedLandValues( aRegionName, aPeriod );

    // Calculating the shares
    for ( unsigned int i = 0; i < mChildren.size(); i++ ) {
        mChildren[ i ]->setInitShares( aRegionName,
                                       mSigma,
                                       mLandAllocation[ aPeriod ],
                                       1, // Share of land use history.
                                       mLandUseHistory.get(),
                                       aPeriod );
    }

    // This is the root node so its share is 100%.
    mShare[ aPeriod ] = 1;
}

double TreeLandAllocator::calcLandShares( const string& aRegionName,
                                          const double aSigmaAbove,
                                          const double aTotalBaseLand,
                                          const int aPeriod ){
    // First adjust value of unmanaged land nodes
    setUnmanagedLandValues( aRegionName, aPeriod );

    LandNode::calcLandShares( aRegionName, aSigmaAbove, aTotalBaseLand, aPeriod );
 
    // This is the root node so its share is 100%.
    mShare[ aPeriod ] = 1;
    return 1;
}

void TreeLandAllocator::calcLandAllocation( const string& aRegionName,
                                            const double aLandAllocationAbove,
                                            const int aPeriod ){
    for ( unsigned int i = 0; i < mChildren.size(); ++i ){
        mChildren[ i ]->calcLandAllocation( aRegionName, mLandAllocation[ aPeriod ], aPeriod );
    }
}

void TreeLandAllocator::calcLUCCarbonFlowsOut( const string& aRegionName, 
                                                   const int aYear ){
    for ( unsigned int i = 0; i < mChildren.size(); ++i ){
        mChildren[ i ]->calcLUCCarbonFlowsOut( aRegionName, aYear );
    }
}

void TreeLandAllocator::calcLUCCarbonFlowsIn( const string& aRegionName, 
                                                     const int aYear ){
    for ( unsigned int i = 0; i < mChildren.size(); ++i ){
        mChildren[ i ]->calcLUCCarbonFlowsIn( aRegionName, aYear );
    }
}

void TreeLandAllocator::calcCarbonBoxModel( const string& aRegionName, 
                                                    const int aYear ){
    for ( unsigned int i = 0; i < mChildren.size(); ++i ){
        mChildren[ i ]->calcCarbonBoxModel( aRegionName, aYear );
    }
}


void TreeLandAllocator::calcFinalLandAllocation( const string& aRegionName,
                                                 const int aPeriod ){
    calcLandShares( aRegionName,
                    0, // No sigma above the root.
                    0, // No land allocation above the root.
                    aPeriod );

    calcLandAllocation( aRegionName,
                        0, // No land allocation above the root.
                        aPeriod );

    const Modeltime* modeltime = scenario->getModeltime();
    const int timeStep = modeltime->gettimestep( aPeriod );
    const int calcYear = modeltime->getper_to_yr( aPeriod );
    // Find any years up to the current period that have not been calculated.
    for( int i = CarbonModelUtils::getStartYear(); i <= calcYear - timeStep; ++i ){
        // If model crashes here on an assert in the YearVector then this is likely because 
        // mCarbonModelStartYear was read in after the first item in the landallocator.
        if( !mCalculated[ i ] ){
            calcFinalLandAllocationHelper( aRegionName, i );
            mCalculated[ i ] = true;
        }
    }
    
    // Always calculate for the years since the previous time step
    int start = calcYear - modeltime->gettimestep( modeltime->getyr_to_per( calcYear ) ) + 1;
    start = max( start, static_cast<int>(CarbonModelUtils::getStartYear()) );
    for( int i = start; i <= calcYear; ++i ){
        calcFinalLandAllocationHelper( aRegionName, i );
    }
}

/*!
 * \brief Helper function to call each phase of land use calculation.
 * \details Land use calculation must be formed in phases for each year.
            This function calls each phase.
 * \param aRegionName the name of the region.
 * \param aYear the year the calculate.
 */
void TreeLandAllocator::calcFinalLandAllocationHelper( const string& aRegionName,
                                                       const int aYear ){
    // The Summerbox must reset the total carbon value
    CarbonSummer::getInstance()->resetState( aYear ); 
    // This must be done first
    calcLUCCarbonFlowsOut( aRegionName, aYear );
    // This must be done second
    calcLUCCarbonFlowsIn( aRegionName, aYear );
    // This must be done third
    calcCarbonBoxModel( aRegionName, aYear );
}

void TreeLandAllocator::setCarbonContent( const string& aLandType,
                                          const string& aProductName,
                                          const double aAboveGroundCarbon,
                                          const double aBelowGroundCarbon,
                                          const int aPeriod )
{
    ALandAllocatorItem* node = findChild( aLandType, eNode );
    if( node ){
        node->setCarbonContent( aLandType, aProductName, aAboveGroundCarbon,
                                aBelowGroundCarbon, aPeriod );
    }
}

double TreeLandAllocator::getUnmanagedCalAveObservedRate( const int aPeriod,
                                                          const string& aType ) const
{
    // Find the subtree where the node resides.
    const ALandAllocatorItem* subTreeRoot = findParentOfType( aType );
    if( !subTreeRoot ){
        // It would only be possible for there to be no root if the type did not
        // exist. Warnings will have already been printed.
        return 1;
    }

    // Starting at the subtree, find the unmanaged land nest.
    // TODO this needs to be changed if unmanaged land were to be in more than
    // one part of any specific land tree
    // (new form for calibration equation would probably need to be derived)
    const ALandAllocatorItem* unmanagedNest
        = findItem<ALandAllocatorItem>( eBFS, subTreeRoot, IsUnmanagedNest() );

    if( !unmanagedNest ){
        ILogger& mainLog = ILogger::getLogger( "main_log" );
        mainLog.setLevel( ILogger::ERROR );
        mainLog << "There was no unmanaged node in the subtree for land type "
                << aType << "." << endl;
        return 1;
    }

    // Starting at the unmanaged land node adjust the calibrated average observed
    // rate working up to the subtree root.
    double aveObservedRate = unmanagedNest->getInstrinsicRate( aPeriod );
    while( unmanagedNest && unmanagedNest != subTreeRoot ){
        double sigmaAbove = unmanagedNest->getParent() ?
                            unmanagedNest->getParent()->getSigma() : 0;
        aveObservedRate /= pow( unmanagedNest->getShare( aPeriod ), sigmaAbove );
        unmanagedNest = unmanagedNest->getParent();
    }

    return aveObservedRate;
}

/*!
 * \brief Returns the conceptual root of the passed in type.
 * \note A conceptual root is a node whose parent's sigma is 0.
 * \param aType The type of node to find a conceptual root for.
 * \return A pointer to the conceptual root of the passed in type.
 */
const ALandAllocatorItem* TreeLandAllocator::findParentOfType( const string& aType ) const {
    
    // Find the requested child.
    const ALandAllocatorItem* item = findChild( aType, eAny );
    
    // Make sure we found the item.
    if( !item ) {
        return 0;
    }

    // Search up the tree until a conceptual root is found. This will search
    // until it finds the child of a node with a sigma of zero or the root node.
    while( item->getParent() && !item->isConceptualRoot() ){
        item = item->getParent();
    }
    return item;
}


void TreeLandAllocator::csvOutput( const string& aRegionName ) const {
    LandNode::csvOutput( aRegionName );
}


void TreeLandAllocator::dbOutput( const string& aRegionName ) const {
    LandNode::dbOutput( aRegionName );
}

void TreeLandAllocator::accept( IVisitor* aVisitor, const int aPeriod ) const {
    LandNode::accept( aVisitor, aPeriod );
}
