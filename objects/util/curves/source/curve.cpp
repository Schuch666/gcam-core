/*! 
* \file curve.cpp
* \ingroup Util
* \brief Curve class source file.
* \author Josh Lurz
* \date $Date$
* \version $Revision$
*/
#include "util/base/include/definitions.h"
#include <iostream>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include "util/base/include/xml_helper.h"
#include "util/curves/include/curve.h"
#include "util/curves/include/point_set_curve.h"

using namespace std;

const string Curve::XML_NAME = "Curve";

//! Constructor
Curve::Curve(){
    numericalLabel = 0;
}

//! Copy Constructor
Curve::Curve( const Curve& curveIn ){
}

//! Destructor.
Curve::~Curve(){
}

//! Equals operator.
bool Curve::operator==( const Curve& rhs ) const {
    return( getSortedPairs() == rhs.getSortedPairs() );
}

//! Not Equals operator.
bool Curve::operator!=( const Curve& rhs ) const {
    return !( *this == rhs );
}

//! Static function to return the name of the XML element associated with this object.
const string& Curve::getXMLNameStatic() {
    return XML_NAME;
}

//! Return the name of the XML element associated with this object.
const string& Curve::getXMLName() const {
    return XML_NAME;
}

//! Static factory method which returns the correct type of Curve based on the type string.
Curve* Curve::getCurve( const string& type ) {
    Curve* curve;

    if( type == PointSetCurve::getXMLNameStatic() ){
        curve = new PointSetCurve();
    }
    else {
        cout << "Invalid type of " << getXMLNameStatic() << " requested: " << type << "." << endl;
        curve = 0;
    }
    return curve;
}

//! Print out the curve to an XML File
void Curve::toInputXML( ostream& out, Tabs* tabs ) const {
    XMLWriteOpeningTag( Curve::getXMLNameStatic(), out, tabs, name, 0, getXMLName() );
    XMLWriteElementCheckDefault( title, "title", out, tabs );
    XMLWriteElementCheckDefault( numericalLabel, "numericalLabel", out, tabs );
    XMLWriteElementCheckDefault( xAxisLabel, "xAxisLabel", out, tabs );
    XMLWriteElementCheckDefault( yAxisLabel, "yAxisLabel", out, tabs );
    XMLWriteElementCheckDefault( xAxisUnits, "xAxisUnit", out, tabs );
    XMLWriteElementCheckDefault( yAxisUnits, "yAxisUnit", out, tabs );
    toInputXMLDerived( out, tabs );
    XMLWriteClosingTag( Curve::getXMLNameStatic(), out, tabs );
}

//! Parse a curve from a DOM tree.
void Curve::XMLParse( const xercesc::DOMNode* node ) {
    
    name = XMLHelper<string>::getAttrString( node, "name" );
    xercesc::DOMNode* curr = 0;
    xercesc::DOMNodeList* nodeList; 
    string nodeName;
    
    // assume node is valid.
    assert( node );

    // get all children of the node.
    nodeList = node->getChildNodes();
    
    // loop through the children
    for ( int i = 0; i < static_cast<int>( nodeList->getLength() ); i++ ){
        curr = nodeList->item( i );
        nodeName = XMLHelper<void>::safeTranscode( curr->getNodeName() );

        // select the type of node.
        if( nodeName == "#text" ) {
            continue;
        }
        else if ( nodeName == "title" ){
            title = XMLHelper<string>::getValueString( curr );
        } 
        else if ( nodeName == "numericalLabel" ){
            numericalLabel = XMLHelper<double>::getValue( curr );
        } 
        else if ( nodeName == "xAxisLabel" ){
            xAxisLabel = XMLHelper<string>::getValueString( curr );
        } 
        else if ( nodeName == "yAxisLabel" ){
            yAxisLabel = XMLHelper<string>::getValueString( curr );
        } 
        else if ( nodeName == "xAxisUnits" ){
            xAxisUnits = XMLHelper<string>::getValueString( curr );
        } 
        else if ( nodeName == "yAxisUnits" ){
            yAxisUnits = XMLHelper<string>::getValueString( curr );
        } 
        else if ( XMLParseDerived( curr ) ){
            // Do nothing, action was taken care of in the derived parse. 
        }
        else {
            cout << "Unrecognized text string: " << nodeName << " found while parsing Curve." << endl;
        }
    }
}

//! Get the curve name.
const std::string Curve::getName() const {
    return name;
}

//! Get the curve title.
const std::string Curve::getTitle() const {
    return title;
}

//! Set the curve title.
void Curve::setTitle( const std::string& titleIn ){
    title = titleIn;
}

//! Get the numerical value or label associated with this curve.
double Curve::getNumericalLabel() const {
    return numericalLabel;
}

//! Set the numerical value or label associated with this curve.
void Curve::setNumericalLabel( const double numericalLabelIn ) {
    numericalLabel = numericalLabelIn;
}

//! Get the X axis label.
const std::string Curve::getXAxisLabel() const {
    return xAxisLabel;
}

//! Get the y axis label.
const std::string Curve::getYAxisLabel() const {
    return yAxisLabel;
}

//! Set the X axis label.
void Curve::setXAxisLabel( const std::string& xLabelIn ) {
    xAxisLabel = xLabelIn;
}

//! Set the Y axis label. 
void Curve::setYAxisLabel( const std::string& yLabelIn ) {
    yAxisLabel = yLabelIn;
}

//! Get the X axis units.
const std::string Curve::getXAxisUnits() const {
    return xAxisUnits;
}

//! Get the Y axis units.
const std::string Curve::getYAxisUnits() const {
    return yAxisUnits;
}

//! Set the X axis units.
void Curve::setXAxisUnits( const std::string& xUnitsIn ){
    xAxisUnits = xUnitsIn;
}

//! Set the Y axis units.
void Curve::setYAxisUnits( const std::string& yUnitsIn ){
    yAxisUnits = yUnitsIn;
}

//! Calculate the hamming distance between this Curve and another Curve over a given time interval and step.
double Curve::getHammingDistance( const Curve* otherCurve, const double xStart, const double xEnd, const double xInterval ) const {
    double sum = 0;
    for( double xVal = xStart; xVal <= xEnd; xVal += xInterval ){
        double thisY = getY( xVal );
        double otherY = otherCurve->getY( xVal );
        double difference = fabs( thisY - otherY );
        sum += difference;
    }
    return sum;
}
