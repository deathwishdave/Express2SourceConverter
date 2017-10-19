/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#include <set>
#include <string>
#include <boost/optional.hpp>
#include "schema/ExpressToSourceConverter.h"
#include "schema/entity/Attribute.h"
#include "schema/type/Type.h"
#include "Entity.h"

Entity::Entity( ExpressToSourceConverter* generator )
{
	m_generator = generator;
	m_entity_name = "";
	m_schema = "";
	m_EXPRESS_supertype_name = "";
	m_supertype = nullptr;
}

Entity::~Entity() {}

bool Entity::setSchema( QString schema )
{
	m_schema = schema;
	return true;
}

void Entity::setSchemaVersion( QString schema_version )
{
	m_schema_version = schema_version;
}

void Entity::parseEntity()
{
	QString schema = m_schema;
	QString header_str;

	// extract header: go to first semicolon
	{
		int pos_header_end;
		QRegExp rx_header_end( ";" );
		if( (pos_header_end = rx_header_end.indexIn( schema, 0 ) ) == -1 )
		{
			m_generator->slotEmitTxtOutWarning( "could not detect header correctly in: " + schema );
		}
		else
		{
			QStringRef header_str_ref(&schema, 0, pos_header_end+rx_header_end.matchedLength() );
			header_str = header_str_ref.toString();

			QRegExp rx_supertype( "SUPERTYPE" );
			if( rx_supertype.indexIn( header_str, 0 ) != -1 )
			{
				QRegExp rx_supertype_of( "(?:ABSTRACT)?\\s*SUPERTYPE\\s*OF\\s*\\(" );
				int supertype_begin = rx_supertype_of.indexIn( header_str, 0 );
				if( supertype_begin != -1 )
				{
					QString complete_match = rx_supertype_of.cap(0);
					int supertype_content_begin = supertype_begin + complete_match.size();

					// SUPERTYPE OF ((ONEOF  (b_spline_curve_with_knots, uniform_curve, quasi_uniform_curve, bezier_curve)) ANDOR rational_b_spline_curve) SUBTYPE OF ...
					int supertype_end = supertype_content_begin;
					int num_open_braces = 1;
					while( supertype_end < header_str.size() )
					{
						if( header_str[supertype_end] == '(' )
						{
							++num_open_braces;
						}
						else
						if( header_str[supertype_end] == ')' )
						{
							--num_open_braces;
							if( num_open_braces == 0 )
							{
								break;
							}
						}
						++supertype_end;
					}
					++supertype_end;
					header_str.remove( supertype_begin, supertype_end - supertype_begin );
				}
				//QRegExp rx_supertype( "(?:ABSTRACT)?\\s*SUPERTYPE\\s*OF\\s*\\(?(?:ONEOF)?\\s*\\([a-zA-Z0-9_,\\s]+\\)?\\s*\\(?(?:ANDOR\\s*[a-zA-Z0-9_,\\s]+)?\\s*\\)" );
				//if( rx_supertype.indexIn( header_str, 0 ) != -1 )
				//{
				//	QString complete_match = rx_supertype.cap(0);
				//	QString supertype = rx_supertype.cap(1);
				//	header_str.remove( rx_supertype );
				//}
				else
				{
					QRegExp rx_supertype( "(?:ABSTRACT)?\\s*SUPERTYPE;" );
					int supertype_begin = rx_supertype.indexIn( header_str, 0 );
					if( supertype_begin != -1 )
					{
						QString complete_match = rx_supertype.cap( 0 );
						int length_supertype = rx_supertype.matchedLength();
						header_str.remove( supertype_begin, length_supertype );
					}
					else
					{
						m_generator->slotEmitTxtOutWarning( "could not detect SUPERTYPE: " + header_str );
					}
				}
			}
			
			QRegExp rx_subtype( "SUBTYPE", Qt::CaseInsensitive );
			if( rx_subtype.indexIn( header_str, 0 ) != -1 )
			{
				rx_subtype.setPattern( "SUBTYPE\\s*OF\\s*\\(\\s*([a-zA-Z0-9_]+)\\s*\\);" );
				if( rx_subtype.indexIn( header_str, 0 ) != -1 )
				{
					QString complete_match = rx_subtype.cap(0);
					QString supertype = rx_subtype.cap(1);
					m_EXPRESS_supertype_name = supertype;
					header_str.remove( rx_subtype );
				}
				else
				{
					m_generator->slotEmitTxtOutWarning( "could not detect SUBTYPE OF correctly in: " + schema );
				}
			}
			
			QRegExp rx_entity( "\\s*ENTITY\\s*[a-zA-Z0-9_]+\\s*;?\\s*" );
			if( rx_entity.indexIn( header_str, 0 ) != -1 )
			{
				header_str.remove( rx_entity );
			}

			if( header_str.size() > 0 )
			{
				m_generator->slotEmitTxtOutWarning( "remaining header: " + header_str + " in schema: " + schema );
			}
		}
		
		// cut out header
		schema.remove( 0, pos_header_end + rx_header_end.matchedLength() );
	}

	//DERIVE
	//P : LIST [3:3] OF IfcDirection := IfcBuildAxes(Axis, RefDirection);
	//U : LIST [2:2] OF IfcDirection := IfcBaseAxis(2,SELF\IfcCartesianTransformationOperator.Axis1,
    //     SELF\IfcCartesianTransformationOperator.Axis2,?);
	// ClosedCurve : LOGICAL := Segments[NSegments].Transition <> Discontinuous;
	//SELF\IfcGeometricRepresentationContext.WorldCoordinateSystem : IfcAxis2Placement := ParentContext.WorldCoordinateSystem;
	//NumberOfHeadings : INTEGER := SIZEOF(QUERY( Temp <* Rows | Temp.IsHeading));
	//P:LIST[3:3]OFSfaDirection:=SfaBuildAxes(Axis,RefDirection);
	//E0 : SfaPressureMeasure := 2.0*CompressionStrength/CompressionStrain; 

	QRegExp rx_derive( "\\s*DERIVE\\s*\\n*" );
	int pos_derive = rx_derive.indexIn( schema, 0 );
	if( pos_derive != -1 )
	{
		schema.remove( rx_derive );
	
		QString derive_name( "[a-zA-Z0-9_\\.\\\\]+" );
		QString derive_type( "[a-zA-Z0-9_\\.\\[\\]:\\s\\?\\\\]+" );
		QString derive_expression( "[a-zA-Z0-9_\\-\\[\\]:,\\?<>\\.\\|\\s=\\(\\)\\/\\*'\\\\]+" );
		QRegExp rx_derive_line( "\\s*(" + derive_name + ")\\s*:\\s*(" + derive_type + ")\\s*:=\\s*(" + derive_expression + ");\\s*" );
		int pos_derive_line;
		while( pos_derive_line = rx_derive_line.indexIn( schema, pos_derive ) != -1 )
		{
			QString derive_line = rx_derive_line.cap(0);
			schema.remove( rx_derive_line );
		}
	}
	

	//WHERE
	//WR1 : SELF\IfcPlacement.Location.Dim = 3;
	//WR2 : (NOT (EXISTS (Axis))) OR (Axis.Dim = 3);
	//WR3 : (NOT (EXISTS (RefDirection))) OR (RefDirection.Dim = 3);
	//WR4 : (NOT (EXISTS (Axis))) OR (NOT (EXISTS (RefDirection))) OR (IfcCrossProduct(Axis,RefDirection).Magnitude > 0.0);
	//WR5 : NOT ((EXISTS (Axis)) XOR (EXISTS (RefDirection)));
	//WHERE CorrespondingDisplacements:(SIZEOF(Displacements)=SIZEOF(SELF\SfaCorrespondingStructuralNode.Coordinates))
	//E0:SfaPressureMeasure:=2.0*CompressionStrength/CompressionStrain;
	{
		QRegExp rx_where( "\\s*WHERE\\s*\\n" );
		int pos_where = rx_where.indexIn( schema, 0 );
		
		if( pos_where != -1 )
		{
			QRegExp rx_single_where( "\\s*([a-zA-Z0-9_]+)\\s*:\\s*([a-zA-Z0-9_\\-\\[\\]\\(\\){}:,\\+\\/\\s=\\|\\.\\(\\)\\<\\>\\'\\*\\\\]+);\\n*" );
			int pos_single_where = pos_where;
			while( pos_single_where = rx_single_where.indexIn( schema, pos_single_where ) != -1 )
			{
				QString single_where_complete = rx_single_where.cap( 0 );
				schema.remove( single_where_complete );
				pos_single_where = pos_where;
				//pos_single_where += rx_single_where.matchedLength();
			}

			//int pos_where_end = schema.indexOf( ";", pos_where );
			int length_where = rx_where.matchedLength();// pos_where_end - pos_where + 1;
			schema.remove( pos_where, length_where );
		}
	}
	//{
	//	QString single_where( "\\s*([a-zA-Z0-9_]+)\\s*:\\s*([a-zA-Z0-9_\\-\\[\\]\\(\\){}:,\\+\\/\\s=\\|\\.\\(\\)\\<\\>\\'\\*\\\\]+);\\n*" );
	//	QRegExp rx_where( "\\s*WHERE\\s*\\n(" + single_where + ")+" );
	//	int pos_where = rx_where.indexIn( schema, 0 );

	//	if( pos_where != -1 )
	//	{
	//		schema.remove( rx_where );
	//	}
	//}

	//INVERSE
	//Contains : SET [0:?] OF IfcClassificationItem FOR ItemOf;
	//UsingCurves : SET [1:?] OF IfcCompositeCurve FOR Segments;
	{
		QString single_inverse = "\\s*([a-zA-Z0-9_]+)\\s*:\\s*([a-zA-Z0-9_\\?\\[\\]:\\s=\\(\\)]+);\\n*";
		QRegExp rx_inverse( "\\s*INVERSE\\s*\\n(" + single_inverse + ")+" );
		int pos_inverse = rx_inverse.indexIn( schema, 0 );
		if( pos_inverse != -1 )
		{
			parseInverseAttributes( rx_inverse.cap(0) );
			schema.remove( rx_inverse );
		}
	}
	//UNIQUE
	//Contains : SET [0:?] OF IfcClassificationItem FOR ItemOf;
	{
		QString rx_unique_single( "\\s*([a-zA-Z0-9_]+)\\s*:\\s*([a-zA-Z0-9_\\s\\[\\]:,\\?]+);\\n*" );
		QRegExp rx_unique( "\\s*UNIQUE\\s*\\n(" + rx_unique_single + ")+" );
		int pos_unique = rx_unique.indexIn( schema, 0 );
		if( pos_unique != -1 )
		{
			QString unique_attributes = rx_unique.cap(0);
			schema.remove( rx_unique );

		}
	}

	// attributes: lines like
	// Item : IfcGeometricRepresentationItem;
	// InnerBoundaries : OPTIONAL SET [1:?] OF IfcCurve;
	// Pixel : LIST [1:?] OF BINARY(32);
	int pos_attribute=0;
	QRegExp rx_attribute( "\\s*([a-zA-Z0-9_]+)\\s*:\\s*([a-zA-Z0-9_\\s\\[\\]:\\?\\(\\)]+);\\n*" );
	while( (pos_attribute = rx_attribute.indexIn( schema, 0 ) ) != -1 )
	{
		QString complete_match = rx_attribute.cap(0);
		QString attribute_name = rx_attribute.cap(1);
		QString attribute_type_cpp = rx_attribute.cap(2);
		QString attribute_type_EXPRESS = "";
		//QString attribute_type_cpp;
		attribute_type_cpp.remove( QRegExp( "^(\\s*)*" ) );

		bool attribute_optional = false;
		QString attribute_comment( "" );
		QRegExp rx_optional( "(OPTIONAL\\s+)[a-zA-Z0-9_]+" );
		if( rx_optional.indexIn( attribute_type_cpp, 0 ) != -1 )
		{
			attribute_optional = true;
			attribute_comment = "optional";
			attribute_type_cpp.remove( rx_optional.cap(1) );
		}

		bool primitive_attribute = false;
		for( auto it_basic_types=m_generator->m_basic_types.begin(); it_basic_types!=m_generator->m_basic_types.end(); ++it_basic_types )
		{
			QString type_express = it_basic_types->first;
			QString type_cpp = it_basic_types->second->m_CPP_typename;
			QRegExp rx_express( "(^\\s*)?" + type_express );
			if( rx_express.indexIn( attribute_type_cpp, 0 ) != -1 )
			{
				//attribute_type_cpp = attribute_type_EXPRESS;
				attribute_type_EXPRESS = type_express;
				attribute_type_cpp.replace( rx_express, type_cpp );
				primitive_attribute = true;
			}
		}

		Attribute::Cardinality cardinality = Attribute::CARDINALITY_SINGLE;

		QRegExp rx_array( "^(OPTIONAL|UNIQUE\\s)?(ARRAY|LIST|SET)\\s" );
		if( rx_array.indexIn( attribute_type_cpp, 0 ) != -1 )
		{
			//Segments : LIST [1:?] OF IfcCompositeCurveSegment;
			//RelatedConstraints : LIST [1:?] OF UNIQUE IfcConstraint;
			int pos = 0;
			int count_vec_dim = 0;
			QRegExp rx_vec_dim( "^(OPTIONAL|UNIQUE\\s)?(ARRAY|LIST|SET)\\s\\[([0-9]+):([0-9\\?]+)\\]\\sOF\\s*" );
			while( (pos = rx_vec_dim.indexIn( attribute_type_cpp, 0 ) ) != -1 )
			{
				QString optional_unique	= rx_vec_dim.cap(1);
				QString container_type	= rx_vec_dim.cap(2);
				QString min				= rx_vec_dim.cap(3);
				QString max				= rx_vec_dim.cap(4);

				attribute_type_cpp.remove( rx_vec_dim );
				++count_vec_dim;

				if( optional_unique.compare("OPTIONAL") == 0 )
				{
					attribute_optional = true;
				}
			}
			
			// TODO:		if( min == 0 && max == 1 )   => not a vector, but a simple pointer attribute
			// example:		Decomposes	 :	SET [0:1] OF IfcRelAggregates FOR RelatedObjects;

			if( count_vec_dim == 1 )
			{
				cardinality = Attribute::CARDINALITY_VECTOR;
			}
			else if( count_vec_dim == 2 )
			{
				cardinality = Attribute::CARDINALITY_VECTOR_2D;
			}
			else if( count_vec_dim == 3 )
			{
				cardinality = Attribute::CARDINALITY_VECTOR_3D;
			}
			else
			{
				m_generator->slotEmitTxtOutWarning( "ARRAY/LIST with dim > 2 not implemented: " + schema );
				return;
			}

			//QRegExp rx( "(ARRAY|LIST|SET)\\s\\[([0-9]+):([0-9\\?]+)\\]\\sOF(\\sUNIQUE)?\\s([a-zA-Z0-9_\\s\\*]+)([0-9\\(\\)]*)?$" );
			QRegExp rx( "(UNIQUE\\s)?([a-zA-Z0-9:_\\s\\*]+)([0-9\\(\\)]*)?$" );
			if( (pos = rx.indexIn( attribute_type_cpp, 0 ) ) == -1 )
			{
				m_generator->slotEmitTxtOutWarning( "couln't understand ARRAY/LIST: " + attribute_type_cpp );
				return;
			}

			QString complete_match = rx.cap(0);
			QString unique = rx.cap(1);
			QString datatype = rx.cap(2);

			attribute_type_cpp = datatype;
		}

		attribute_type_cpp.remove( QRegExp( "^\\s*" ) );

		shared_ptr<Attribute>  att;
		
		if( primitive_attribute )
		{
			if( attribute_type_EXPRESS.size() > 0 )
			{
				att = shared_ptr<AttributePrimitive>( new AttributePrimitive( attribute_type_EXPRESS, attribute_type_cpp, attribute_name, attribute_comment ) );
			}
		}
		else
		{
			QString key = attribute_type_cpp;

			if( m_generator->m_map_types.find( key ) != m_generator->m_map_types.end() )
			{
				att = shared_ptr<AttributeType>( new AttributeType( m_generator->m_map_types[key], attribute_name, attribute_comment ) );
			}
			else if( m_generator->m_map_entities.find( key ) != m_generator->m_map_entities.end() )
			{
				att = shared_ptr<AttributeEntity>( new AttributeEntity( m_generator->m_map_entities[key], attribute_name, attribute_comment ) );
			}
			else
			{
				m_generator->slotEmitTxtOutWarning( "Entity::setSchema: unkown attribute type: " + attribute_type_cpp );
			}
		}
		schema.remove( complete_match );
		if( att != nullptr )
		{
			att->m_optional = attribute_optional;
			att->m_cardinality = cardinality;
			m_attributes.push_back( att );
		}
	}

	schema.remove( QRegExp( "[\\r\\n\\s]" ) );
	if( schema.size() > 0 )
	{
		m_generator->slotEmitTxtOutWarning( "remaining schema after parsing: " + schema );
	}

	if( m_generator->m_basic_types.find( m_EXPRESS_supertype_name ) != m_generator->m_basic_types.end() )
	{
		m_generator->slotEmitTxtOutWarning( "Entity::parse: elementary supertype of entity: " + header_str );
		return;
	}
	else if( m_EXPRESS_supertype_name.startsWith(  "ARRAY" ) || m_EXPRESS_supertype_name.startsWith( "LIST" ) || m_EXPRESS_supertype_name.startsWith( "SET" ) )
	{
		m_generator->slotEmitTxtOutWarning( "Entity::parse: couln't understand ARRAY/LIST supertype: " + m_EXPRESS_supertype_name );
		return;
	}
	else if( m_EXPRESS_supertype_name.startsWith( "STRING" ) )
	{
		m_generator->slotEmitTxtOutWarning( "Entity::parse: couln't understand STRING supertype: " + m_EXPRESS_supertype_name );
		return;
	}
	else if( m_EXPRESS_supertype_name.startsWith( "ENUMERATION" ) )
	{
		m_generator->slotEmitTxtOutWarning( "Entity::parse: couln't understand ENUMERATION supertype: " + m_EXPRESS_supertype_name );
		return;
	}
	else if( m_EXPRESS_supertype_name.startsWith( "SELECT" ) )
	{
		m_generator->slotEmitTxtOutWarning( "Entity::parse: couln't understand SELECT supertype: " + m_EXPRESS_supertype_name );
		return;
	}
	else if( m_EXPRESS_supertype_name == "" )
	{
		
	}
	else if( m_generator->m_map_entities.find( m_EXPRESS_supertype_name ) != m_generator->m_map_entities.end() )
	{
		m_supertype = m_generator->m_map_entities[m_EXPRESS_supertype_name];
		if( m_EXPRESS_supertype_name.compare( m_supertype->m_entity_name ) !=0 )
		{
			m_generator->slotEmitTxtOutWarning( "Entity::parse: m_supertype_name != m_supertype->m_className in " + m_entity_name );
		}
	}
	else 
	{
		m_generator->slotEmitTxtOutWarning( "Entity::parse: unknown supertype: " + m_EXPRESS_supertype_name );
		return;
	}

	return;
}

void Entity::parseInverseAttributes( QString inv_code )
{
	QRegExp rx_inv( "^(\\s)*INVERSE\\s*" );
	if( ( rx_inv.indexIn( inv_code, 0 ) ) == -1 )
	{
		return;
	}
	inv_code.remove( rx_inv );

	int pos_attribute=0;
	QRegExp rx_attribute_inverse( "\\s*([a-zA-Z0-9_]+)\\s*:\\s*([a-zA-Z0-9_\\s\\[\\]:\\?\\(\\)]+)\\s+FOR\\s+([a-zA-Z0-9_\\[\\]:\\?\\(\\)]+);\\n*" );
	//QRegExp rx_attribute( "\\s*([a-zA-Z0-9_]+)\\s*:\\s*([a-zA-Z0-9_\\s\\[\\]:\\?\\(\\)]+);\\n*" );
	while( (pos_attribute = rx_attribute_inverse.indexIn( inv_code, 0 ) ) != -1 )
	{
		QString complete_match = rx_attribute_inverse.cap(0);
		QString attribute_name = rx_attribute_inverse.cap(1) + "_inverse";
		QString attribute_type = rx_attribute_inverse.cap(2);
		QString for_counterpart_attribute_name = rx_attribute_inverse.cap(3);
		attribute_type.remove( QRegExp( "^(\\s*)*" ) );

		bool attribute_optional = false;
		QString attribute_comment;
		QRegExp rx_optional( "(OPTIONAL\\s+)[a-zA-Z0-9_]+" );
		if( rx_optional.indexIn( attribute_type, 0 ) != -1 )
		{
			attribute_comment = "optional";
			attribute_type.remove( rx_optional.cap(1) );
			attribute_optional = true;
		}

		bool primitive_attribute = false;
		
		for( auto it=m_generator->m_basic_types.begin(); it!=m_generator->m_basic_types.end(); ++it )
		{
			QString type_express = it->first;
			QString type_cpp = it->second->m_CPP_typename;
			QRegExp rx_express( "(^\\s*)?" + type_express );
			if( rx_express.indexIn( attribute_type, 0 ) != -1 )
			{
				attribute_type.replace( rx_express, type_cpp );
				primitive_attribute = true;
			}
		}

		Attribute::Cardinality cardinality = Attribute::CARDINALITY_SINGLE;

		QRegExp rx_array( "(ARRAY|LIST|SET)\\s" );
		if( rx_array.indexIn( attribute_type, 0 ) != -1 )
		{
			//Segments : LIST [1:?] OF IfcCompositeCurveSegment;
			//RelatedConstraints : LIST [1:?] OF UNIQUE IfcConstraint;
			int pos = 0;
			QRegExp rx( "(ARRAY|LIST|SET)\\s\\[([0-9]+):([0-9\\?]+)\\]\\sOF(\\sUNIQUE)?\\s([a-zA-Z0-9_\\s\\*]+)([0-9\\(\\)]*)?$" );
			if( (pos = rx.indexIn( attribute_type, 0 ) ) == -1 )
			{
				m_generator->slotEmitTxtOutWarning( "couln't understand ARRAY/LIST: " + attribute_type );
				return;
			}
			QString complete_match = rx.cap(0);
			QString container_type = rx.cap(1);
			QString min = rx.cap(2);
			QString max = rx.cap(3);
			QString unique = rx.cap(4);
			QString datatype = rx.cap(5);
			
			QRegExp rx_num( "\\([0-9]*\\)" );
			if( rx_num.indexIn( datatype ) != -1 )
			{
			}

			attribute_type = datatype;
			cardinality = Attribute::CARDINALITY_VECTOR;
		}

		attribute_type.remove( QRegExp( "^\\s*" ) );

		QRegExp rx_for( "([a-zA-Z0-9_]+)\\s*FOR\\s*([a-zA-Z0-9_]*)\\s*" );
		if( ( rx_for.indexIn( attribute_type, 0 ) ) != -1 )
		{
			attribute_comment += attribute_type;
			attribute_type = rx_for.cap( 1 );
		}
		attribute_type.remove( QRegExp( "^\\s*" ) ); // remove leading whitespaces
		attribute_type.remove( QRegExp( "\\s*$" ) ); // remove trailing whitespaces

		if( primitive_attribute )
		{
			m_generator->slotEmitTxtOutWarning( "primitive inverse attribute: " + attribute_type ); 
		}
		else
		{
			QString key = attribute_type;
			if( m_generator->m_map_types.find( key ) != m_generator->m_map_types.end() )
			{
				m_generator->slotEmitTxtOutWarning( "inverse TYPE attribute: " + attribute_type ); 
			}
			else if( m_generator->m_map_entities.find( key ) != m_generator->m_map_entities.end() )
			{
				shared_ptr<AttributeInverse> att_inverse( new AttributeInverse( m_generator->m_map_entities[key], attribute_name, for_counterpart_attribute_name, attribute_comment ) );
				att_inverse->m_cardinality = cardinality;
				att_inverse->m_optional = attribute_optional;
				m_inverse_attributes.push_back( att_inverse );
			}
			else
			{
				m_generator->slotEmitTxtOutWarning( "Entity::setSchema: unkown attribute type: " + attribute_type );
			}
		}

		inv_code.remove( complete_match );
	}

	return;
}

void Entity::getAllSuperClasses( QStringList& all_super_classes )
{
	const QString IfcPP = m_generator->getFilenamePrefix();
	if( m_supertype != nullptr )
	{
		all_super_classes.append( m_supertype->m_entity_name );
		m_supertype->getAllSuperClasses( all_super_classes );
	}
	else
	{
		// as Entity, we need to be at least derived from IfcPPEntity
		all_super_classes.append( "IfcPPEntity" );
	}
	all_super_classes.removeDuplicates();
}

void Entity::getAllAttributes( std::vector<shared_ptr<Attribute> >& vec_attributes, bool include_inherited )
{
	if( include_inherited )
	{
		if( m_supertype != nullptr )
		{
			m_supertype->getAllAttributes( vec_attributes, include_inherited );
		}
	}

	std::copy( m_attributes.begin(), m_attributes.end(), std::back_inserter( vec_attributes ) );
	std::copy( m_inverse_attributes.begin(), m_inverse_attributes.end(), std::back_inserter( vec_attributes ) );
}

// attribute declaration
void Entity::getAllAttributesNames( QStringList& set_all_attributes, bool include_inherited )
{
	if( include_inherited )
	{
		if( m_supertype != nullptr )
		{
			m_supertype->getAllAttributesNames( set_all_attributes, include_inherited );
		}
	}

	// complex attributes need to come first in header file
	std::vector<shared_ptr<Attribute> > attributes_all( m_attributes );
	std::copy( m_inverse_attributes.begin(), m_inverse_attributes.end(), std::back_inserter(attributes_all));

	for( size_t ii=0; ii<attributes_all.size(); ++ii )
	{
		shared_ptr<Attribute>  att = attributes_all[ii];
		if( dynamic_pointer_cast<AttributeType>( att ) )
		{
			shared_ptr<AttributeType>  att_type = dynamic_pointer_cast<AttributeType>( att );
			shared_ptr<Type>  t = att_type->m_type;
			set_all_attributes.append(t->m_className);
		}
		else if( dynamic_pointer_cast<AttributeEntity>( att ) )
		{
			shared_ptr<AttributeEntity>  att_entity = dynamic_pointer_cast<AttributeEntity>( att );
			shared_ptr<Entity>  ent( att_entity->m_entity );
			set_all_attributes.append(ent->m_entity_name);
		}
	}
	set_all_attributes.removeDuplicates();
}

Attribute::Cardinality Entity::getCardinalityOfAttributeName( QString attribute_name )
{
	std::vector<shared_ptr<Attribute> > attributes_all( m_attributes );
	std::copy( m_inverse_attributes.begin(), m_inverse_attributes.end(), std::back_inserter(attributes_all));

	for( size_t ii=0; ii<attributes_all.size(); ++ii )
	{
		shared_ptr<Attribute>  att = attributes_all[ii];
		if(attribute_name.compare(att->m_name)== 0)
		{
			return att->m_cardinality;
		}
	}
	return Attribute::CARDINALITY_UNDEFINED;
}

shared_ptr<Attribute> Entity::getAttributeWithName( QString attribute_name )
{
	std::vector<shared_ptr<Attribute> > attributes_all( m_attributes );
	std::copy( m_inverse_attributes.begin(), m_inverse_attributes.end(), std::back_inserter(attributes_all));
	for( size_t ii=0; ii<attributes_all.size(); ++ii )
	{
		shared_ptr<Attribute>  att = attributes_all[ii];
		if(attribute_name.compare(att->m_name)== 0)
		{
			return att;
		}
	}
	return shared_ptr<Attribute>();
}

bool Entity::hasBoostOptional()
{
	std::vector<shared_ptr<Attribute> > vec_attributes;
	getAllAttributes( vec_attributes, false );
	for( size_t ii = 0; ii<vec_attributes.size(); ++ii )
	{
		shared_ptr<Attribute>  att = vec_attributes[ii];
		if( att->hasBoostOptional() )
		{
			return true;
		}
	}
	return false;
}

QString Entity::getCodeHeader()
{
	const QString IfcPP = m_generator->getFilenamePrefix();
	m_select_supertypes.removeDuplicates();
	QStringList set_select_superclasses;
	QString add_ifcpp = m_generator->m_add_ifcpp_in_path ? "ifcpp/" : "";

	QString entity_inherit;
	std::map<QString,QString> additional_includes;

	if( m_select_supertypes.size() > 0 )
	{
		foreach( QString sel, m_select_supertypes )
		{
			if( m_generator->m_map_types.find(sel) != m_generator->m_map_types.end() )
			{
				shared_ptr<Type>  t = m_generator->m_map_types[sel];
				t->getAllSuperClasses(set_select_superclasses);
			}
			if( m_generator->m_map_entities.find(sel) != m_generator->m_map_entities.end() )
			{
				shared_ptr<Entity>  e = m_generator->m_map_entities[sel];
				e->getAllSuperClasses(set_select_superclasses);
			}
		}

		foreach( QString sel, m_select_supertypes )
		{
			if( !set_select_superclasses.contains(sel) )
			{
				if( entity_inherit.size() > 0 )
				{
					entity_inherit += ", virtual public ";
				}
				else
				{
					entity_inherit += " : virtual public ";
				}
				entity_inherit += sel;


				if( m_generator->m_map_types.find(sel) != m_generator->m_map_types.end())
				{
					shared_ptr<Type>  t = m_generator->m_map_types[sel];
					additional_includes[t->m_className] = t->m_className;
				}
				else if( m_generator->m_map_entities.find(sel) != m_generator->m_map_entities.end())
				{
					shared_ptr<Entity>  e = m_generator->m_map_entities[sel];
					additional_includes[e->m_entity_name] = e->m_entity_name;
				}
			}
		}
	}

	QString code_h( m_generator->m_header_comment );
	code_h += "#pragma once\n";
	
	code_h += "#include <vector>\n";
	code_h += "#include <map>\n";
	code_h += "#include <sstream>\n";
	code_h += "#include <string>\n";
	if( hasBoostOptional() )
	{
		code_h += "#include <boost/optional.hpp>\n";
	}
	
	code_h += "#include \"" + add_ifcpp + "model/IfcPPBasicTypes.h\"\n";
	code_h += "#include \"" + add_ifcpp + "model/IfcPPObject.h\"\n";
	code_h += "#include \"" + add_ifcpp + "model/IfcPPGlobal.h\"\n";

	for( auto it_includes=additional_includes.begin(); it_includes!=additional_includes.end(); ++it_includes)
	{
		QString include_file = it_includes->second;
		code_h += "#include \"" + include_file + ".h\"\n";
	}
	
	if( m_supertype != nullptr )
	{
		code_h += "#include \"" + m_supertype->m_entity_name + ".h\"\n";
	}

	if( m_generator->m_schema_namespace.length() > 0 )
	{
		code_h += "namespace " + m_generator->m_schema_namespace + "\n{\n";
	}

	// attribute declaration
	QStringList set_forward_declare_classes;
	getAllAttributesNames( set_forward_declare_classes, false );
	int num_all_attributes = getNumAttributesIncludingSupertypes();

	foreach( QString className, set_forward_declare_classes )
	{
		code_h += "class IFCPP_EXPORT " + className + ";\n";
	}

	if( m_supertype )
	{
		if( entity_inherit.size() > 0 )
		{
			entity_inherit += ", public " + m_supertype->m_entity_name;
		}
		else
		{
			entity_inherit += " : public " + m_supertype->m_entity_name;
		}

	}
	else
	{
		if( !set_select_superclasses.contains( "IfcPPEntity") )
		{
			if( entity_inherit.size() > 0 )
			{
				entity_inherit += ", public IfcPPEntity";
			}
			else
			{
				entity_inherit += " : public IfcPPEntity";
			}
		}
	}
	

	// class definition
	code_h += "//ENTITY\n";
	code_h += "class IFCPP_EXPORT " + m_entity_name + entity_inherit + "\n{ \n";

	// enums
	code_h += "public:\n";

	// constructors
	code_h += m_entity_name + "();\n";
	code_h += m_entity_name + "( int id );\n";

	// destructor
	code_h += "~" + m_entity_name + "();\n";

	// methods
	code_h += "virtual shared_ptr<IfcPPObject> getDeepCopy( IfcPPCopyOptions& options );\n";
	code_h += "virtual void getStepLine( std::stringstream& stream ) const;\n";
	code_h += "virtual void getStepParameter( std::stringstream& stream, bool is_select_type = false ) const;\n";
	code_h += "virtual void readStepArguments( const std::vector<std::wstring>& args, const std::map<int,shared_ptr<IfcPPEntity> >& map );\n";
	code_h += "virtual void setInverseCounterparts( shared_ptr<IfcPPEntity> ptr_self );\n";
	code_h += "virtual size_t getNumAttributes() { return " + QString::number( num_all_attributes ) + "; }\n";
	code_h += "virtual void getAttributes( std::vector<std::pair<std::string, shared_ptr<IfcPPObject> > >& vec_attributes );\n";
	code_h += "virtual void getAttributesInverse( std::vector<std::pair<std::string, shared_ptr<IfcPPObject> > >& vec_attributes );\n";
	code_h += "virtual void unlinkFromInverseCounterparts();\n";

	if( m_generator->m_code_reset_attributes )
	{
		code_h += "virtual void resetAttributes();\n";
		code_h += "virtual void resetAttributesInverse();\n";
	}

	// className()
	code_h += "virtual const char* className() const { return \"" + m_entity_name + "\"; }\n";
	code_h += "virtual const std::wstring toString() const;\n\n";

	int max_strlen = getStrlenAttributesTypes();
	code_h += getAttributeDeclaration( max_strlen );

	// end of class
	code_h += "};\n";

	if( m_generator->m_schema_namespace.length() > 0 )
	{
		code_h += "}\n";//namespace
	}
	code_h += "\n";

	return code_h;
}

void Entity::getCodeGetAttributes( QString& code_get_att, QString& code_get_att_inv )
{
	const QString IfcPP = m_generator->getFilenamePrefix();
	QString namespace_cpp;
	if( m_generator->m_schema_namespace.length() > 0 )
	{
		namespace_cpp = m_generator->m_schema_namespace + "::";
	}

	code_get_att = "";
	code_get_att += "void " + namespace_cpp + m_entity_name + "::getAttributes( std::vector<std::pair<std::string, shared_ptr<IfcPPObject> > >& vec_attributes )\n";
	code_get_att += "{\n";

	code_get_att_inv = "";
	code_get_att_inv += "void " + namespace_cpp + m_entity_name + "::getAttributesInverse( std::vector<std::pair<std::string, shared_ptr<IfcPPObject> > >& vec_attributes_inverse )\n";
	code_get_att_inv += "{\n";

	if( m_supertype )
	{
		code_get_att += namespace_cpp + m_supertype->m_entity_name + "::getAttributes( vec_attributes );\n";
		code_get_att_inv += namespace_cpp + m_supertype->m_entity_name + "::getAttributesInverse( vec_attributes_inverse );\n";
	}

	// get list of attributes
	std::vector<shared_ptr<Attribute> > attributes_all( m_attributes );
	std::copy( m_inverse_attributes.begin(), m_inverse_attributes.end(), std::back_inserter(attributes_all));

	for( int i=0; i<attributes_all.size(); ++i )
	{
		shared_ptr<Attribute>  att = attributes_all[i];
		
		if( att->m_cardinality == Attribute::CARDINALITY_SINGLE )
		{
			shared_ptr<AttributeType> att_type = dynamic_pointer_cast<AttributeType>( att );
			if( att_type )
			{
				code_get_att += "vec_attributes.push_back( std::make_pair( \"" + att->m_name + "\", m_" + att->m_name + " ) );\n";

				if( att_type->m_type->m_select.size() > 0 )
				{
					//code_cpp += "std::vector<shared_ptr<" + IfcPP + "Object> > vec_" + att->m_name + ";\n";
					//code_cpp += "vec_" + att->m_name + ".puch_back( dynamic_pointer_cast<shared_ptr<" + IfcPP + "Object> >( m_" + att->m_name + ") );\n";
					//code_cpp += "vec_attributes.push_back( vec_" + att->m_name + " );\n";
					continue;
				}
			}

			shared_ptr<AttributeEntity> att_entity = dynamic_pointer_cast<AttributeEntity>(att);
			if( att_entity )
			{
				if( dynamic_pointer_cast<AttributeInverse>(att) )
				{
					code_get_att_inv += "vec_attributes_inverse.push_back( std::make_pair( \"" + att->m_name + "\", shared_ptr<IfcPPEntity>( m_" + att->m_name + " ) ) );\n";
				}
				else
				{
					code_get_att += "vec_attributes.push_back( std::make_pair( \"" + att->m_name + "\", m_" + att->m_name + " ) );\n";
				}
				continue;
			}
			
			shared_ptr<AttributePrimitive> att_primitive = dynamic_pointer_cast<AttributePrimitive>(att);
			if( att_primitive )
			{
				QString IfcPP_primitive_type = "";
				if( m_generator->m_basic_types.find( att_primitive->m_primitive_type_EXPRESS ) != m_generator->m_basic_types.end() )
				{
					IfcPP_primitive_type = m_generator->m_basic_types[att_primitive->m_primitive_type_EXPRESS]->m_IFCPP_typename;
				}
				else
				{
					m_generator->slotEmitTxtOutWarning( "unknown primitive attribute: " + att_primitive->m_primitive_type_cpp );
				}

				if( att_primitive->hasBoostOptional() )
				{
					code_get_att += "if( m_" + att->m_name + " )\n";
					code_get_att += "{\n";
					code_get_att += "vec_attributes.push_back( std::make_pair( \"" + att->m_name + "\", shared_ptr<IfcPP" + IfcPP_primitive_type + "Attribute>( new IfcPP" + IfcPP_primitive_type + "Attribute( m_" + att->m_name + ".get() ) ) ) );\n";
					code_get_att += "}\n";
					code_get_att += "else\n";
					code_get_att += "{\n";
					code_get_att += "vec_attributes.push_back( std::make_pair( \"" + att->m_name + "\", shared_ptr<IfcPP" + IfcPP_primitive_type + "Attribute>() ) );\t // empty shared_ptr\n";  // empty shared_ptr
					code_get_att += "}\n";
				}
				else
				{
					code_get_att += "vec_attributes.push_back( std::make_pair( \"" + att->m_name + "\", shared_ptr<IfcPP" + IfcPP_primitive_type + "Attribute>( new IfcPP" + IfcPP_primitive_type + "Attribute( m_" + att->m_name + " ) ) ) );\n";
				}
			}
		}
		else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
		{
			QString att_typename;

			shared_ptr<AttributePrimitive> att_primitive = dynamic_pointer_cast<AttributePrimitive>(att);
			if( att_primitive )
			{
				QString IfcPP_primitive_type = "";

				if( m_generator->m_basic_types.find( att_primitive->m_primitive_type_EXPRESS ) != m_generator->m_basic_types.end() )
				{
					IfcPP_primitive_type = m_generator->m_basic_types[att_primitive->m_primitive_type_EXPRESS]->m_IFCPP_typename;
				}
				else
				{
					m_generator->slotEmitTxtOutWarning( "unknown primitive attribute: " + att_primitive->m_primitive_type_cpp );
				}

				code_get_att += "if( m_" + att->m_name + ".size() > 0 )\n";
				code_get_att += "{\n";
				code_get_att += "shared_ptr<IfcPPAttributeObjectVector> " + att->m_name + "_vec_obj( new IfcPPAttributeObjectVector() );\n";
				code_get_att += "for( size_t i=0; i<m_" + att->m_name + ".size(); ++i )\n";
				code_get_att += "{\n";
				code_get_att += att->m_name + "_vec_obj->m_vec.push_back( shared_ptr<IfcPP" + IfcPP_primitive_type + "Attribute>( new IfcPP" + IfcPP_primitive_type + "Attribute(m_" + att->m_name + "[i] ) ) );\n";
				code_get_att += "}\n";
				code_get_att += "vec_attributes.push_back( std::make_pair( \"" + att->m_name + "\", " +att->m_name + "_vec_obj ) );\n";
				code_get_att += "}\n";

				continue;
			}

			//
			shared_ptr<AttributeType> att_type = dynamic_pointer_cast<AttributeType>( att );
			if( att_type )
			{
				att_typename = att_type->m_type->m_className;
			}

			shared_ptr<AttributeEntity> att_entity = dynamic_pointer_cast<AttributeEntity>( att );
			if( att_entity )
			{
				shared_ptr<Entity> att_entity_ptr( att_entity->m_entity );
				att_typename = att_entity_ptr->m_entity_name;


			}

			if( dynamic_pointer_cast<AttributeInverse>(att) )
			{
				code_get_att_inv += "if( m_" + att->m_name + ".size() > 0 )\n";
				code_get_att_inv += "{\n";
				code_get_att_inv += "shared_ptr<IfcPPAttributeObjectVector> " + att->m_name + "_vec_obj( new IfcPPAttributeObjectVector() );\n";
				code_get_att_inv += "for( size_t i=0; i<m_" + att->m_name + ".size(); ++i )\n";
				code_get_att_inv += "{\n";
				code_get_att_inv += "if( !m_" + att->m_name + "[i].expired() )\n";
				code_get_att_inv += "{\n";
				code_get_att_inv += att->m_name + "_vec_obj->m_vec.push_back( shared_ptr<" + att_typename + ">( m_" + att->m_name + "[i] ) );\n";
				code_get_att_inv += "}\n";
				code_get_att_inv += "}\n";
				code_get_att_inv += "vec_attributes_inverse.push_back( std::make_pair( \"" + att->m_name + "\", " +att->m_name + "_vec_obj ) );\n";
				code_get_att_inv += "}\n";
			}
			else
			{
				code_get_att += "if( m_" + att->m_name + ".size() > 0 )\n";
				code_get_att += "{\n";
				code_get_att += "shared_ptr<IfcPPAttributeObjectVector> " + att->m_name + "_vec_object( new IfcPPAttributeObjectVector() );\n";
				code_get_att += "std::copy( m_" + att->m_name + ".begin(), m_" + att->m_name + ".end(), std::back_inserter( " + att->m_name + "_vec_object->m_vec ) );\n";
				code_get_att += "vec_attributes.push_back( std::make_pair( \"" + att->m_name + "\", " + att->m_name + "_vec_object ) );\n";
				code_get_att += "}\n";
			}
		}
	}
	code_get_att += "}\n";
	code_get_att_inv += "}\n";
}

void Entity::getCodeResetAttributes( QString& code_reset_att, QString& code_reset_att_inv )
{
	const QString IfcPP = m_generator->getFilenamePrefix();
	QString namespace_cpp;
	if( m_generator->m_schema_namespace.length() > 0 )
	{
		namespace_cpp = m_generator->m_schema_namespace + "::";
	}

	code_reset_att = "";
	code_reset_att += "void " + namespace_cpp + m_entity_name + "::resetAttributes()\n";
	code_reset_att += "{\n";

	code_reset_att_inv = "";
	code_reset_att_inv += "void " + namespace_cpp + m_entity_name + "::resetAttributesInverse()\n";
	code_reset_att_inv += "{\n";

	if( m_supertype )
	{
		code_reset_att += namespace_cpp + m_supertype->m_entity_name + "::resetAttributes();\n";
		code_reset_att_inv += namespace_cpp + m_supertype->m_entity_name + "::resetAttributesInverse();\n";
	}

	// get list of attributes
	std::vector<shared_ptr<Attribute> > attributes_all( m_attributes );
	std::copy( m_inverse_attributes.begin(), m_inverse_attributes.end(), std::back_inserter(attributes_all));

	for( int i=0; i<attributes_all.size(); ++i )
	{
		shared_ptr<Attribute>  att = attributes_all[i];
		
		if( att->m_cardinality == Attribute::CARDINALITY_SINGLE )
		{
			shared_ptr<AttributeType> att_type = dynamic_pointer_cast<AttributeType>( att );
			if( att_type )
			{
				code_reset_att += "m_" + att->m_name + ".reset();\n";

				if( att_type->m_type->m_select.size() > 0 )
				{
					//code_cpp += "std::vector<shared_ptr<" + IfcPP + "Object> > vec_" + att->m_name + ";\n";
					//code_cpp += "vec_" + att->m_name + ".puch_back( dynamic_pointer_cast<shared_ptr<" + IfcPP + "Object> >( m_" + att->m_name + ") );\n";
					//code_cpp += "vec_attributes.push_back( vec_" + att->m_name + " );\n";
					continue;
				}
			}

			shared_ptr<AttributeEntity> att_entity = dynamic_pointer_cast<AttributeEntity>(att);
			if( att_entity )
			{
				if( dynamic_pointer_cast<AttributeInverse>(att) )
				{
					code_reset_att_inv += "m_" + att->m_name + ".reset();\n";
				}
				else
				{
					code_reset_att += "m_" + att->m_name + ".reset();\n";
				}
				continue;
			}
		}
		else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
		{
			QString att_typename;

			shared_ptr<AttributePrimitive> att_primitive = dynamic_pointer_cast<AttributePrimitive>(att);
			if( att_primitive )
			{
				QString IfcPP_primitive_type = "";

				if( m_generator->m_basic_types.find( att_primitive->m_primitive_type_EXPRESS ) != m_generator->m_basic_types.end() )
				{
					IfcPP_primitive_type = m_generator->m_basic_types[att_primitive->m_primitive_type_EXPRESS]->m_IFCPP_typename;
				}
				else
				{
					m_generator->slotEmitTxtOutWarning( "unknown primitive attribute: " + att_primitive->m_primitive_type_cpp );
				}

				code_reset_att += "m_" + att->m_name + ".clear();\n";
				continue;
			}

			shared_ptr<AttributeType> att_type = dynamic_pointer_cast<AttributeType>( att );
			if( att_type )
			{
				att_typename = att_type->m_type->m_className;
			}

			shared_ptr<AttributeEntity> att_entity = dynamic_pointer_cast<AttributeEntity>( att );
			if( att_entity )
			{
				shared_ptr<Entity> att_entity_ptr( att_entity->m_entity );
				att_typename = att_entity_ptr->m_entity_name;
			}

			if( dynamic_pointer_cast<AttributeInverse>(att) )
			{
				code_reset_att_inv += "m_" + att->m_name + ".clear();\n";
			}
			else
			{
				code_reset_att += "m_" + att->m_name + ".clear();\n";
			}
		}
	}
	code_reset_att += "}\n";
	code_reset_att_inv += "}\n";

}

QString Entity::getCodeCpp()
{
	QString class_name_upper = m_entity_name.toUpper();
	QString code_cpp( m_generator->m_header_comment );
	const QString filename_prefix = m_generator->getFilenamePrefix();
	QString subfolder_ifc_classes = m_generator->getSubfolderIfcClasses();
	QString add_ifcpp = m_generator->m_add_ifcpp_in_path ? "ifcpp/" : "";
	
	code_cpp += "#include <sstream>\n";
	code_cpp += "#include <limits>\n\n";
	code_cpp += "#include \"" + add_ifcpp + "model/IfcPPException.h\"\n";
	code_cpp += "#include \"" + add_ifcpp + "model/IfcPPAttributeObject.h\"\n";
	code_cpp += "#include \"" + add_ifcpp + "model/IfcPPGuid.h\"\n";
	code_cpp += "#include \"" + add_ifcpp + "reader/ReaderUtil.h\"\n";
	code_cpp += "#include \"" + add_ifcpp + "writer/WriterUtil.h\"\n";
	//code_cpp += "#include \"" + add_ifcpp + subfolder_ifc_classes + filename_prefix + "EntityFactory.h\"\n";
	
	// attribute declaration
	QStringList set_attribute_includes;
	getAllAttributesNames( set_attribute_includes, true );

	std::map<QString,QString> additional_includes;

	foreach( QString className, set_attribute_includes )
	{
		additional_includes[className] = className;
	}

	for( int i=0; i<m_inverse_counterparts.size(); ++i )
	{
		shared_ptr<InverseCounterpart> inverse_counterpart( m_inverse_counterparts[i] );
		QString counterpart_class = inverse_counterpart->m_counterpart_class;
		additional_includes[counterpart_class] = counterpart_class;
	}
	additional_includes[m_entity_name] = m_entity_name;

	std::map<QString,QString>::iterator it_includes;
	for( it_includes=additional_includes.begin(); it_includes!=additional_includes.end(); ++it_includes)
	{
		QString include_file = it_includes->second;
		code_cpp += "#include \"" + add_ifcpp + subfolder_ifc_classes + "include/" + include_file + ".h\"\n";
	}
	
	code_cpp += "\n";

	QString namespace_cpp;
	if( m_generator->m_schema_namespace.length() > 0 )
	{
		namespace_cpp = m_generator->m_schema_namespace + "::";
	}

	// comment with class name
	code_cpp += "// ENTITY " + m_entity_name + " ";
	while( code_cpp.length() <= 80 )
	{
		code_cpp += "/";
	}
	code_cpp += "\n";

	// constructor
	code_cpp += namespace_cpp + m_entity_name + "::" + m_entity_name + "() {}\n";
	code_cpp += namespace_cpp + m_entity_name + "::" + m_entity_name + "( int id ) { m_id = id; }\n";

	// destructor
	code_cpp += namespace_cpp + m_entity_name + "::~" + m_entity_name + "() {}\n";

	code_cpp += "shared_ptr<IfcPPObject> " + namespace_cpp + m_entity_name + "::getDeepCopy( IfcPPCopyOptions& options )\n{\n";
	code_cpp += "shared_ptr<" + m_entity_name + "> copy_self( new " + m_entity_name + "() );\n";
	code_cpp += getCodeDeepCopy();
	code_cpp += "return copy_self;\n";
	code_cpp += "}\n";

	// step writer
	code_cpp += "void " + namespace_cpp + m_entity_name + "::getStepLine( std::stringstream& stream ) const\n{\n";
	code_cpp += "	stream << \"#\" << m_id << \"= " + m_entity_name.toUpper() + "\" << \"(\";\n";
	code_cpp += getCodeWriterEntity( false );
	code_cpp += "	stream << \");\";\n}\n";


	code_cpp += "void " + namespace_cpp + m_entity_name + "::getStepParameter( std::stringstream& stream, bool ) const { stream << \"#\" << m_id; }\n";
	code_cpp += "const std::wstring " + namespace_cpp + m_entity_name + "::toString() const { return L\"" + m_entity_name + "\"; }\n";
	code_cpp += "void " + namespace_cpp + m_entity_name + "::readStepArguments( const std::vector<std::wstring>& args, const std::map<int,shared_ptr<IfcPPEntity> >& map )\n{\n";
	
	int iarg = 0;
	code_cpp += getEntityReader( true, iarg );
	code_cpp += "}\n";


	QString code_get_att;
	QString code_get_att_inv;
	getCodeGetAttributes( code_get_att, code_get_att_inv );
	code_cpp += code_get_att;
	code_cpp += code_get_att_inv;

	//bool code_reset_attributes = true;
	if( m_generator->m_code_reset_attributes )
	{
		QString code_reset_att;
		QString code_reset_att_inv;
		getCodeResetAttributes( code_reset_att, code_reset_att_inv );
		code_cpp += code_reset_att;
		code_cpp += code_reset_att_inv;
	}

	// inverse counterparts
	shared_ptr<Entity> supertype = m_supertype;
	QString unlink_from_inverse = "";
	QString call_supertype_inverse_counterparts = "";
	if( supertype )
	{
		call_supertype_inverse_counterparts += supertype->m_entity_name + "::setInverseCounterparts( ptr_self_entity );\n";
	}


	// unlink from inverse counterparts
	unlink_from_inverse += "void " + namespace_cpp + m_entity_name + "::unlinkFromInverseCounterparts()\n{\n";
	if( supertype )
	{
		unlink_from_inverse += supertype->m_entity_name + "::unlinkFromInverseCounterparts();\n";
	}

	if( m_inverse_counterparts.size() == 0 )
	{
		if( call_supertype_inverse_counterparts.length() > 0 )
		{
			code_cpp += "void " + namespace_cpp + m_entity_name + "::setInverseCounterparts( shared_ptr<IfcPPEntity> ptr_self_entity )\n{\n";
		}
		else
		{
			code_cpp += "void " + namespace_cpp + m_entity_name + "::setInverseCounterparts( shared_ptr<IfcPPEntity> )\n{\n";
		}
		
		code_cpp += call_supertype_inverse_counterparts;
		code_cpp += "}\n";
	}
	else
	{
		code_cpp += "void " + namespace_cpp + m_entity_name + "::setInverseCounterparts( shared_ptr<IfcPPEntity> ptr_self_entity )\n{\n";
		code_cpp += call_supertype_inverse_counterparts;
		code_cpp += "shared_ptr<" + m_entity_name + "> ptr_self = dynamic_pointer_cast<" + m_entity_name + ">( ptr_self_entity );\n";
		code_cpp += "if( !ptr_self ) { throw IfcPPException( \"" + m_entity_name + "::setInverseCounterparts: type mismatch\" ); }\n";

		std::map<QString, std::vector<shared_ptr<InverseCounterpart> > > map_inverse_counterparts;
		std::map<QString, shared_ptr<InverseCounterpart> >::iterator it_inverse_counterparts;
		for( size_t i=0; i<m_inverse_counterparts.size(); ++i )
		{
			shared_ptr<InverseCounterpart> inverse_counterpart( m_inverse_counterparts[i] );
			QString counterpart_for = inverse_counterpart->m_for;
			if( map_inverse_counterparts.find(counterpart_for) == map_inverse_counterparts.end() )
			{
				map_inverse_counterparts[counterpart_for] = std::vector<shared_ptr<InverseCounterpart> >();
			}
			map_inverse_counterparts[counterpart_for].push_back( inverse_counterpart );
		}


		std::map<QString, std::vector<shared_ptr<InverseCounterpart> > >::iterator it_map_counterparts;
		for( it_map_counterparts=map_inverse_counterparts.begin(); it_map_counterparts!=map_inverse_counterparts.end(); ++it_map_counterparts )
		{
			std::vector<shared_ptr<InverseCounterpart> >& vec_counterparts =  it_map_counterparts->second;
			
			QString counterpart_for = it_map_counterparts->first;
			shared_ptr<Attribute> own_attribute_for = getAttributeWithName( counterpart_for );
			shared_ptr<AttributeType> own_attribute_for_type = dynamic_pointer_cast<AttributeType>(own_attribute_for);
			QString own_attribute_for_datatype;
			if( own_attribute_for_type )
			{
				own_attribute_for_datatype = own_attribute_for_type->m_type->m_className;
			}

			shared_ptr<AttributeEntity> own_attribute_for_entity = dynamic_pointer_cast<AttributeEntity>(own_attribute_for);
			if( own_attribute_for_entity )
			{
				shared_ptr<Entity> own_attribute_for_entity_entity( own_attribute_for_entity->m_entity );
				own_attribute_for_datatype = own_attribute_for_entity_entity->m_entity_name;
			}

			Attribute::Cardinality cardinality = getCardinalityOfAttributeName( counterpart_for );
			
			QString counterpart_for_loop_in = "";
			QString counterpart_for_loop_out = "";
			QString counterpart_for_index = "";
			if( cardinality == Attribute::CARDINALITY_SINGLE )
			{
				// no need to go through a loop
			}
			else if( cardinality == Attribute::CARDINALITY_VECTOR )
			{
				counterpart_for_index = "[i]";
				counterpart_for_loop_in = "	for( size_t i=0; i<m_" + counterpart_for + ".size(); ++i )\n{\n";
				counterpart_for_loop_out = "}\n";
			}
			else
			{
				m_generator->slotEmitTxtOutWarning( "inverse attribute counterpart, unexpected cardinality. className: " + m_entity_name );
			}
			QString pointer_to_counter_attribute = "m_" + counterpart_for + counterpart_for_index;

			code_cpp += counterpart_for_loop_in;

			// unlink
			unlink_from_inverse += counterpart_for_loop_in;

			for( int i=0; i<vec_counterparts.size(); ++i )
			{
				shared_ptr<InverseCounterpart> inverse_counterpart( vec_counterparts[i] );
				QString counterpart_name = inverse_counterpart->m_counterpart_name;
				QString counterpart_type = inverse_counterpart->m_counterpart_type;
				QString counterpart_class = inverse_counterpart->m_counterpart_class;
				Attribute::Cardinality counterpart_cardinality = inverse_counterpart->m_counterpart_cardinality;

				QString cast_code_in = "";
				QString cast_code_out = "";

				if( own_attribute_for_datatype.compare(counterpart_class)!= 0)
				{
					QString casted_name = counterpart_for + "_" + counterpart_class;
					cast_code_in += "shared_ptr<" + counterpart_class + ">  " + casted_name + " = dynamic_pointer_cast<" + counterpart_class + ">( m_"  + counterpart_for + counterpart_for_index + " );\n";
					pointer_to_counter_attribute = casted_name;
				}

				// unlink
				unlink_from_inverse += cast_code_in;
				unlink_from_inverse += "if( " + pointer_to_counter_attribute + " )\n{\n";

				QString counterpart_assign_ptr_self = "";
				if( counterpart_cardinality == Attribute::CARDINALITY_SINGLE )
				{
					counterpart_assign_ptr_self = " = ptr_self;\n";

					// unlink
					unlink_from_inverse += "shared_ptr<" + counterpart_type + "> self_candidate( " + pointer_to_counter_attribute + "->m_" + counterpart_name + " );\n";
					unlink_from_inverse += "if( self_candidate.get() == this )\n";
					unlink_from_inverse += "{\n";
					unlink_from_inverse += "weak_ptr<" + counterpart_type + ">& self_candidate_weak = " + pointer_to_counter_attribute + "->m_" + counterpart_name + ";\n";
					unlink_from_inverse += "self_candidate_weak.reset();\n";
					unlink_from_inverse += "}\n";

				}
				else if( counterpart_cardinality == Attribute::CARDINALITY_VECTOR )
				{
					counterpart_assign_ptr_self = ".push_back( ptr_self );\n";


					// unlink
					unlink_from_inverse += "std::vector<weak_ptr<" + counterpart_type + "> >& " + counterpart_name + " = " + pointer_to_counter_attribute + "->m_" + counterpart_name + ";\n";
					unlink_from_inverse += "for( auto it_" + counterpart_name + " = " + counterpart_name + ".begin(); it_" + counterpart_name + " != " + counterpart_name + ".end(); )\n";
					unlink_from_inverse += "{\n";
					unlink_from_inverse += "shared_ptr<" + counterpart_type + "> self_candidate( *it_" + counterpart_name + " );\n";
					unlink_from_inverse += "if( self_candidate.get() == this )\n";
					unlink_from_inverse += "{\n";
					unlink_from_inverse += "it_" + counterpart_name + "= " + counterpart_name + ".erase( it_" + counterpart_name + " );\n";
					unlink_from_inverse += "}\n";
					unlink_from_inverse += "else\n";
					unlink_from_inverse += "{\n";
					unlink_from_inverse += "++it_" + counterpart_name + ";\n";
					unlink_from_inverse += "}\n";
					unlink_from_inverse += "}\n";

				}

				code_cpp += cast_code_in;
				code_cpp += "if( " + pointer_to_counter_attribute + " )\n{\n";
				code_cpp += pointer_to_counter_attribute + "->m_" + counterpart_name + counterpart_assign_ptr_self;
				code_cpp += "}\n";
				code_cpp += cast_code_out;

				// unlink
				unlink_from_inverse += cast_code_out;
				unlink_from_inverse += "}\n";
			}
			code_cpp += counterpart_for_loop_out;

			// unlink
			unlink_from_inverse += counterpart_for_loop_out;
		}

		code_cpp += "}\n";
	}
	
	// unlink
	unlink_from_inverse += "}\n";
	code_cpp += unlink_from_inverse;

	//QString entity_name_upper = m_entity_name.toUpper();
	//code_cpp += "static " + filename_prefix + "EntityFactoryHelper<" + namespace_cpp + m_entity_name + "> factory_helper( \"" + entity_name_upper + "\" );\n";

	return code_cpp;
}

void Entity::linkDependencies()
{
	std::vector<shared_ptr<Attribute> > attributes_all( m_attributes );
	std::copy( m_inverse_attributes.begin(), m_inverse_attributes.end(), std::back_inserter(attributes_all));

	for( int i=0; i<attributes_all.size(); ++i )
	{
		shared_ptr<Attribute>  att = attributes_all[i];
		if( dynamic_pointer_cast<AttributeType>( att ) )
		{
			shared_ptr<AttributeType>  att_type = dynamic_pointer_cast<AttributeType>( att );
			shared_ptr<Type>  t = att_type->m_type;
		}
		else if( dynamic_pointer_cast<AttributeEntity>( att ) )
		{
			shared_ptr<AttributeEntity>  att_entity = dynamic_pointer_cast<AttributeEntity>( att );
			shared_ptr<Entity>  e( att_entity->m_entity );

		}
	}


	std::map<QString,shared_ptr<Entity> >::iterator it_entity = m_generator->m_map_entities.find( m_entity_name );
	if( it_entity == m_generator->m_map_entities.end() )
	{
		m_generator->slotEmitTxtOutWarning( "datatype not existing: " + m_entity_name ); 
	}
	shared_ptr<Entity> shared_this_ptr = it_entity->second;
	weak_ptr<Entity> shared_this_ptr_weak( shared_this_ptr );

	for( int i=0; i<m_inverse_attributes.size(); ++i )
	{
		shared_ptr<AttributeInverse>  att_inverse = m_inverse_attributes[i];
		QString inverse_for =  att_inverse->m_for_counterpart;
		QString att_inverse_name = att_inverse->m_name;

		shared_ptr<Entity>  e_inv( att_inverse->m_entity );
		QString att_inverse_datatype = e_inv->m_entity_name;

		it_entity = m_generator->m_map_entities.find( att_inverse_datatype );
		if( it_entity == m_generator->m_map_entities.end() )
		{
			m_generator->slotEmitTxtOutWarning( "inverse attribute datatype not existing: " + att_inverse_datatype ); 
		}

		shared_ptr<Entity> counterpart_entity = it_entity->second;

		// add "this" as inverse attribute to counterpart
		shared_ptr<InverseCounterpart> counter( new InverseCounterpart() );
		counter->m_counterpart_name = att_inverse_name;
		counter->m_counterpart_type = att_inverse_datatype;
		counter->m_for = inverse_for;
		counter->m_counterpart_cardinality = att_inverse->m_cardinality;
		counter->m_counterpart_class = m_entity_name;
		counterpart_entity->m_inverse_counterparts.push_back( counter );

	}

}

QString Entity::getAttributeDeclaration( int max_length_attributes, bool only_comment )
{
	QString declaration;
	if( m_supertype != nullptr )
	{
		//int max_length_attributes_supertype = m_supertype->getStrlenAttributesTypes();
		declaration += m_supertype->getAttributeDeclaration( max_length_attributes, true );
	}


	declaration += "\n// " + m_entity_name + " -----------------------------------------------------------\n";

	for( size_t ii=0; ii < m_attributes.size()+m_inverse_attributes.size(); ++ii )
	{
		bool inverse = false;
		shared_ptr<Attribute>  attrib;
		if( ii < m_attributes.size() )
		{
			attrib = m_attributes[ii];
			if( ii == 0 )
			{
				declaration += "// attributes:\n";;
			}
		}
		else
		{
			size_t i_inv = ii - m_attributes.size();
			attrib = m_inverse_attributes[i_inv];
			if( ii == m_attributes.size() )
			{
				// first inverse attribute
				if( only_comment )
				{
					declaration += "// inverse attributes:\n";
				}
				else
				{
					declaration += "// inverse attributes:\n";
				}
			}
			inverse = true;
		}

		bool use_weak_ptr = inverse;
		QString datatype = attrib->getDataTypeWithSmartPointer(use_weak_ptr);

		QString dec_datatype;
		if( attrib->m_cardinality == Attribute::CARDINALITY_VECTOR )
		{
			dec_datatype = "std::vector<" + datatype + " >";
		}
		else if( attrib->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
		{
			dec_datatype = "std::vector<std::vector<" + datatype + " > >";
		}
		else if( attrib->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
		{
			dec_datatype = "std::vector<std::vector<std::vector<" + datatype + " > > >";
		}
		else
		{
			dec_datatype = datatype;
		}

		if( only_comment )
		{
			if( inverse )
			{
				declaration += "//  " + dec_datatype;
			}
			else
			{
				declaration += "//  " + dec_datatype;
			}
		}
		else
		{
			declaration += dec_datatype;
		}

		int num_chars_max = max_length_attributes;
		int chars_consumed = dec_datatype.size();
		int num_chars_left = chars_consumed % 4;

		// append at least one tab:
		declaration += "\t";
		chars_consumed += 4 - num_chars_left;

		// fill with tabs until max
		while( chars_consumed < num_chars_max + 1 )
		{
			declaration += "\t";
			chars_consumed += 4;
		}
		declaration += "m_" + attrib->m_name + ";";

		if( attrib->m_comment != "" )
		{
			int num_tabs_comment = 7 - ceil( attrib->m_name.size()/4.0 );
			num_tabs_comment = std::max( 1, num_tabs_comment);
			QString indent_comment;
			for( int i_tab=0; i_tab<num_tabs_comment; ++i_tab ) indent_comment += "\t";
			declaration += indent_comment + "//" + attrib->m_comment;

		}
		declaration += "\n";
	}
	return declaration;
}

int Entity::getNumAttributesIncludingSupertypes()
{
	int num = 0;
	if( m_supertype != nullptr )
	{
		num += m_supertype->getNumAttributesIncludingSupertypes();
	}
	num += (int)m_attributes.size();
	return num;
}

int Entity::getStrlenAttributesTypesAndNames()
{
	int size = 1;
	for( int i=0; i<m_attributes.size()+m_inverse_attributes.size(); ++i )
	{
		bool inverse = false;
		shared_ptr<Attribute>  att;
		if( i < m_attributes.size() )
		{
			att = m_attributes[i];
		}
		else
		{
			inverse = true;
			att = m_inverse_attributes[i-m_attributes.size()];
		}

		bool use_weak_ptr = inverse;
		QString datatype = att->getDataTypeWithSmartPointer(use_weak_ptr);

		QString dec1;
		if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
		{
			dec1 = "std::vector<" + datatype + " >";
		}
		else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
		{
			dec1 = "std::vector<std::vector<" + datatype + " > >";
		}
		else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
		{
			dec1 = "std::vector<std::vector<std::vector<" + datatype + " > > >";
		}
		else
		{
			dec1 = datatype;
		}

		QString attribute_declaration( dec1 + "m_" + att->m_name + ";" );
		if( attribute_declaration.size() >size )
		{
			size = attribute_declaration.size();
		}
	}


	return size;
}
int Entity::getStrlenAttributesTypes()
{
	int size = 1;
	for( int i=0; i<m_attributes.size()+m_inverse_attributes.size(); ++i )
	{
		bool inverse = false;
		shared_ptr<Attribute>  att;
		if( i < m_attributes.size() )
		{
			att = m_attributes[i];
		}
		else
		{
			inverse = true;
			att = m_inverse_attributes[i-m_attributes.size()];
		}

		bool use_weak_ptr = inverse;
		QString datatype = att->getDataTypeWithSmartPointer(use_weak_ptr);

		QString dec1;
		if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
		{
			dec1 = "std::vector<" + datatype + " >";
		}
		else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
		{
			dec1 = "std::vector<std::vector<" + datatype + " > >";
		}
		else
		{
			dec1 = datatype;
		}

		if( dec1.size() >size )
		{
			size = dec1.size();
		}
	}

	if( m_supertype )
	{
		int length_att_supertype = m_supertype->getStrlenAttributesTypes();
		if( length_att_supertype > size )
		{
			return length_att_supertype;
		}
	}
	return size;
}

QString Entity::getCodeDeepCopy()
{
	QString copy_cpp = "";

	// supertype attributes
	if( m_supertype != 0 )
	{
		copy_cpp += m_supertype->getCodeDeepCopy();
	}

	// own attributes
	for( int i=0; i<m_attributes.size(); ++i )
	{
		shared_ptr<Attribute>  att = m_attributes[i];
		QString name = att->m_name;

		bool use_weak_ptr = false;
		QString datatype = att->getDataTypeWithSmartPointer(use_weak_ptr);

		if( dynamic_pointer_cast<AttributePrimitive>( att ) )
		{
			shared_ptr<AttributePrimitive> att_primitive = dynamic_pointer_cast<AttributePrimitive>( att );

			if( att->m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				copy_cpp += "copy_self->m_" + att->m_name + " = m_" + att->m_name + ";\n";
				//copy_cpp += "if( m_" + att->m_name + " ) { copy_self->m_" + att->m_name + " = m_" + att->m_name + "; }\n";
			}
		
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
			{
				copy_cpp += "if( m_" + att->m_name + ".size() > 0 ) { std::copy( m_" + att->m_name + ".begin(), m_" + att->m_name + ".end(), std::back_inserter( copy_self->m_" + att->m_name + " ) ); }\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
			{
				copy_cpp += "if( m_" + att->m_name + ".size() > 0 )\n";
				copy_cpp += "{\n";
				copy_cpp += "copy_self->m_" + att->m_name + ".resize( m_" + att->m_name + ".size() );\n";
				copy_cpp += "for( size_t i = 0; i < m_" + att->m_name + ".size(); ++i )\n";
				copy_cpp += "{\n";
				copy_cpp += "std::copy( m_" + att->m_name + "[i].begin(), m_" + att->m_name + "[i].end(), std::back_inserter( copy_self->m_" + att->m_name + "[i] ) );\n";
				copy_cpp += "}\n";
				copy_cpp += "}\n";
			}

			// IfcRepresentationContext

		}
		else
		{
			QString att_type = att->getDataType();
			if( att->m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				if( att_type.compare( "IfcRepresentationContext" ) == 0 )
				{
					//copy_cpp += "// create only shallow copy for IfcRepresentationContext\n";
					copy_cpp += "if( m_" + att->m_name + " )\n";
					copy_cpp += "{\n";
					copy_cpp += "if( options.shallow_copy_IfcRepresentationContext ) { copy_self->m_" + att->m_name + " = m_" + att->m_name + "; }\n";
					copy_cpp += "else { copy_self->m_" + att->m_name + " = dynamic_pointer_cast<" + att_type + ">( m_" + att->m_name + "->getDeepCopy(options) ); }\n";
					copy_cpp += "}\n";
				}
				else if( att_type.compare( "IfcOwnerHistory" ) == 0 )
				{
					//copy_cpp += "// create only shallow copy for IfcOwnerHistory\n";
					copy_cpp += "if( m_" + att->m_name + " )\n";
					copy_cpp += "{\n";
					copy_cpp += "if( options.shallow_copy_IfcOwnerHistory ) { copy_self->m_" + att->m_name + " = m_" + att->m_name + "; }\n";
					copy_cpp += "else { copy_self->m_" + att->m_name + " = dynamic_pointer_cast<" + att_type + ">( m_" + att->m_name + "->getDeepCopy(options) ); }\n";
					copy_cpp += "}\n";
				}
				else if( att_type.compare( "IfcProfileDef" ) == 0 )
				{
					copy_cpp += "if( m_" + att->m_name + " )\n";
					copy_cpp += "{\n";
					copy_cpp += "if( options.shallow_copy_IfcProfileDef ) { copy_self->m_" + att->m_name + " = m_" + att->m_name + "; }\n";
					copy_cpp += "else { copy_self->m_" + att->m_name + " = dynamic_pointer_cast<" + att_type + ">( m_" + att->m_name + "->getDeepCopy(options) ); }\n";
					copy_cpp += "}\n";
				}
				else if( att_type.compare( "IfcGloballyUniqueId" ) == 0 )
				{
					copy_cpp += "if( m_" + att->m_name + " )\n";
					copy_cpp += "{\n";
					//copy_cpp += "if( options.create_new_IfcGloballyUniqueId ) { copy_self->m_" + att->m_name + " = shared_ptr<IfcGloballyUniqueId>(new IfcGloballyUniqueId( CreateCompressedGuidString22() ) ); }\n";
					copy_cpp += "if( options.create_new_IfcGloballyUniqueId ) { copy_self->m_" + att->m_name + " = shared_ptr<IfcGloballyUniqueId>(new IfcGloballyUniqueId( createGUID32_wstr().c_str() ) ); }\n";
					copy_cpp += "else { copy_self->m_" + att->m_name + " = dynamic_pointer_cast<" + att_type + ">( m_" + att->m_name + "->getDeepCopy(options) ); }\n";
					copy_cpp += "}\n";
				}
				else if( m_entity_name.compare( "IfcLocalPlacement" ) == 0 && att->m_name.compare( "PlacementRelTo" ) == 0 )
				{
					copy_cpp += "if( m_" + att->m_name + " )\n";
					copy_cpp += "{\n";
					copy_cpp += "if( options.shallow_copy_IfcLocalPlacement_PlacementRelTo ) { copy_self->m_" + att->m_name + " = m_" + att->m_name + "; }\n";
					copy_cpp += "else { copy_self->m_" + att->m_name + " = dynamic_pointer_cast<" + att_type + ">( m_" + att->m_name + "->getDeepCopy(options) ); }\n";
					copy_cpp += "}\n";
				}
				else
				{
					copy_cpp += "if( m_" + att->m_name + " ) { copy_self->m_" + att->m_name + " = dynamic_pointer_cast<" + att_type + ">( m_" + att->m_name + "->getDeepCopy(options) ); }\n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
			{
				copy_cpp += "for( size_t ii=0; ii<m_" + att->m_name + ".size(); ++ii )\n";
				copy_cpp += "{\n";
				copy_cpp += "auto item_ii = m_" + att->m_name + "[ii];\n";
				copy_cpp += "if( item_ii )\n{\n";
				copy_cpp += "copy_self->m_" + att->m_name + ".push_back( dynamic_pointer_cast<" + att_type+ ">(item_ii->getDeepCopy(options) ) );\n";
				copy_cpp += "}\n";
				copy_cpp += "}\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
			{
				copy_cpp += "copy_self->m_" + att->m_name + ".resize( m_" + att->m_name + ".size() );\n";
				copy_cpp += "for( size_t ii=0; ii<m_" + att->m_name + ".size(); ++ii )\n";
				copy_cpp += "{\n";
				copy_cpp += "std::vector<" + att->getDataTypeWithSmartPointer() + " >& vec_ii = m_" + att->m_name + "[ii];\n";
				copy_cpp += "std::vector<" + att->getDataTypeWithSmartPointer() + " >& vec_ii_target = copy_self->m_" + att->m_name + "[ii];\n";
				copy_cpp += "for( size_t jj=0; jj<vec_ii.size(); ++jj )\n";
				copy_cpp += "{\n";
				copy_cpp += att->getDataTypeWithSmartPointer() + "& item_jj = vec_ii[jj];\n";
				copy_cpp += "if( item_jj )\n{\n";
				copy_cpp += "vec_ii_target.push_back( dynamic_pointer_cast<" + att_type+ ">( item_jj->getDeepCopy(options) ) );\n";
				copy_cpp += "}\n";
				copy_cpp += "}\n";
				copy_cpp += "}\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
			{
				copy_cpp += "m_" + att->m_name + " = other->m_" + att->m_name + ";\n";
			}
		}
	}

	return copy_cpp;
}

QString Entity::getCodeWriterEntity( bool called_from_subtype )
{
	QString entity_writer = "";

	// supertype attributes
	if( m_supertype != 0 )
	{
		QString supertype_writer = m_supertype->getCodeWriterEntity( true );
		if( supertype_writer.length() > 0 )
		{
			entity_writer += supertype_writer + "	stream << \",\";\n";
		}
	}

	// http://en.wikipedia.org/wiki/ISO_10303-21
	// Unset attribute values are given as "$".
	
	// TODO: non optional lists( should be() instead of $ )

	QString undefined_arg = "\"$\"";
	if( called_from_subtype )
	{
		// Explicit attributes which got re-declared as derived in a subtype are encoded as "*" in the position of the supertype attribute.
		undefined_arg = "\"*\"";
	}

	// own attributes
	for( size_t ii=0; ii<m_attributes.size(); ++ii )
	{
		shared_ptr<Attribute>  att = m_attributes[ii];
		QString name = att->m_name;

		bool use_weak_ptr = false;
		QString datatype = att->getDataTypeWithSmartPointer(use_weak_ptr);

		if( dynamic_pointer_cast<AttributePrimitive>(att) )
		{
			shared_ptr<AttributePrimitive> att_primitive = dynamic_pointer_cast<AttributePrimitive>(att);
			if( att_primitive->m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				if( att_primitive->m_primitive_type_cpp.compare( "bool" ) == 0 )
				{
					entity_writer += "if( m_" + att->m_name + " == false ) { stream << \".F.\"; }\n";
					entity_writer += "else if( m_" + att->m_name + " == true ) { stream << \".T.\"; }\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "LogicalEnum" ) == 0 )
				{
					entity_writer += "if( m_" + att->m_name + " == LOGICAL_FALSE ) { stream << \".F.\"; }\n";
					entity_writer += "else if( m_" + att->m_name + " == LOGICAL_TRUE ) { stream << \".T.\"; }\n";
					entity_writer += "else { stream << \".U.\"; } // LOGICAL_UNKNOWN\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "int" ) == 0 )
				{
					if( att_primitive->hasBoostOptional() )
					{
						entity_writer += "if( m_" + att->m_name + " ) {stream << m_" + att->m_name + ".get(); }\n";
						entity_writer += "else {stream << \"$\"; }\n";
					}
					else
					{
						entity_writer += "stream << m_" + att->m_name + ";\n";
					}
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "double" ) == 0 )
				{
					// if non-optional, the double is a shared_ptr<double> or boost::optional<double>
					//entity_writer += "stream << m_" + att->m_name + boost_optional_get + ";\n";
					if( att_primitive->hasBoostOptional() )
					{
						entity_writer += "if( m_" + att->m_name + " ) {stream << m_" + att->m_name + ".get(); }\n";
						entity_writer += "else {stream << \"$\"; }\n";
					}
					else
					{
						entity_writer += "stream << m_" + att->m_name + ";\n";
					}
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "std::wstring" ) == 0 )
				{
					entity_writer += "stream << encodeStepString( m_" + att->m_name + " );\n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
			{
				// vector of 
				if( att_primitive->m_primitive_type_cpp.compare( "bool" ) == 0 )
				{
					entity_writer += "writeBoolList( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "LogicalEnum" ) == 0 )
				{
					entity_writer += "writeLogicalList( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "int" ) == 0 )
				{
					entity_writer += "writeNumericList( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "double" ) == 0 )
				{
					entity_writer += "writeNumericList( stream, m_" + name + " );\n";

				}
				else if( att_primitive->m_primitive_type_cpp.compare( "const wchar_t*" ) == 0 )
				{
					//entity_writer += "writeConstCharList( stream, m_" + name + " );\n";

					entity_writer += "if( m_" + name + ".size() == 0 )\n";
					entity_writer += "{\n";
					entity_writer += "stream << \"$\";\n";
					entity_writer += "return;\n";
					entity_writer += "}\n";
					entity_writer += "stream << \"(\";\n";
					entity_writer += "for( size_t ii = 0; ii < m_" + name + ".size(); ++ii )\n";
					entity_writer += "{\n";
					entity_writer += "if( ii > 0 )\n";
					entity_writer += "{\n";
					entity_writer += "stream << \",\";\n";
					entity_writer += "}\n";
					entity_writer += "const wchar_t* ch = m_" + name + "[ii];\n";
					entity_writer += "stream << ch;\n";
					entity_writer += "}\n";
					entity_writer += "stream << \")\";\n";

				}
				else if( att_primitive->m_primitive_type_cpp.compare( "std::wstring" ) == 0 )
				{
					entity_writer += "writeStringList( stream, m_" + name + " );\n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
			{
				// vector of 
				if( att_primitive->m_primitive_type_cpp.compare( "bool" ) == 0 )
				{
					entity_writer += "writeBoolList2D( stream, m_" + name + " );\n";
				}
				if( att_primitive->m_primitive_type_cpp.compare( "LogicalEnum" ) == 0 )
				{
					entity_writer += "writeLogicalList2D( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "int" ) == 0 )
				{
					entity_writer += "writeNumericList2D( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "double" ) == 0 )
				{
					entity_writer += "writeNumericList2D( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "const wchar_t*" ) == 0 )
				{
					entity_writer += "writeConstCharList2D( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "std::wstring" ) == 0 )
				{
					entity_writer += "writeStringList2D( stream, m_" + name + " );\n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
			{
				// vector of 
				if( att_primitive->m_primitive_type_cpp.compare( "bool" ) == 0 )
				{
					entity_writer += "writeBoolList3D( stream, m_" + name + " );\n";
				}
				if( att_primitive->m_primitive_type_cpp.compare( "LogicalEnum" ) == 0 )
				{
					entity_writer += "writeLogicalList3D( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "int" ) == 0 )
				{
					entity_writer += "writeNumericList3D( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "double" ) == 0 )
				{
					entity_writer += "writeNumericList3D( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "const wchar_t*" ) == 0 )
				{
					entity_writer += "writeConstCharList3D( stream, m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "std::wstring" ) == 0 )
				{
					entity_writer += "writeStringList3D( stream, m_" + name + " );\n";
				}
			}
		}
		else if( dynamic_pointer_cast<AttributeType>(att) )
		{
			shared_ptr<AttributeType>  att_type = dynamic_pointer_cast<AttributeType>(att);
			shared_ptr<Type> at = att_type->m_type;

			if( att->m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				if( att_type->m_type->m_select.size() > 0 )
				{
					entity_writer += "if( m_" + att->m_name + " ) { m_" + att->m_name + "->getStepParameter( stream, true ); } else { stream << " + undefined_arg + " ; }\n";
				}
				else
				{
					entity_writer += "if( m_" + att->m_name + " ) { m_" + att->m_name + "->getStepParameter( stream ); } else { stream << " + undefined_arg + "; }\n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
			{
				if( att_type->m_type->m_super_primitive_CPP.compare( "double" ) == 0 )
				{
					entity_writer += "writeNumericTypeList( stream, m_" + name + " );\n";
				}
				else if( att_type->m_type->m_super_primitive_CPP.compare( "int" ) == 0 )
				{
					entity_writer += "writeNumericTypeList( stream, m_" + name + " );\n";
				}
				else
				{
					entity_writer += "stream << \"(\";\n";
					entity_writer += "for( size_t ii = 0; ii < m_" + name + ".size(); ++ii )\n";
					entity_writer += "{\n";
					entity_writer += "if( ii > 0 )\n";
					entity_writer += "{\n";
					entity_writer += "	stream << \",\";\n";
					entity_writer += "}\n";

					entity_writer += "const shared_ptr<" + at->m_className + ">& type_object = m_" + name + "[ii];\n";
					entity_writer += "if( type_object )\n";
					entity_writer += "{\n";
					if( att_type->m_type->m_select.size() > 0 )
						entity_writer += "	type_object->getStepParameter( stream, true );\n";
					else
						entity_writer += "	type_object->getStepParameter( stream, false );\n";
					entity_writer += "}\n";
					entity_writer += "else\n";
					entity_writer += "{\n";
					entity_writer += "	stream << \"$\";\n";
					entity_writer += "}\n";
					entity_writer += "}\n";
					entity_writer += "stream << \")\";\n";
						
						
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
			{
				if( att_type->m_type->m_super_primitive_CPP.compare( "double" ) == 0 )
				{
					entity_writer += "writeNumericTypeList2D( stream, m_" + name + " );\n";
				}
				else if( att_type->m_type->m_super_primitive_CPP.compare( "int" ) == 0 )
				{
					entity_writer += "writeNumericTypeList2D( stream, m_" + name + " );\n";
				}
				else
				{
					if( att_type->m_type->m_select.size() > 0 )
					{
						//entity_writer += "writeTypeList2D( stream, m_" + name + ", true );\n";
					}
					else
					{
						//entity_writer += "writeTypeList2D( stream, m_" + name + " );\n";
					}

					entity_writer += "stream << \"(\"; \n";

					entity_writer += "for( size_t ii = 0; ii < m_" + name + ".size(); ++ii )\n";
					entity_writer += "{\n";
					entity_writer += "const std::vector<shared_ptr<" + at->m_className + "> >&inner_vec = m_" + name + "[ii];\n";

					entity_writer += "if( ii > 0 )\n";
					entity_writer += "{\n";
					entity_writer += "stream << \"), (\";\n";
					entity_writer += "}\n";

					entity_writer += "for( size_t jj = 0; jj < inner_vec.size(); ++jj )\n";
					entity_writer += "{\n";
					entity_writer += "if( jj > 0 )\n";
					entity_writer += "{\n";
					entity_writer += "stream << \", \";\n";
					entity_writer += "}\n";

					entity_writer += "const shared_ptr<" + at->m_className + ">& type_object = inner_vec[jj];\n";
					entity_writer += "if( type_object )\n";
					entity_writer += "{\n";

					if( att_type->m_type->m_select.size() > 0 )
							entity_writer += "type_object->getStepParameter( stream, true );\n";
					else
						entity_writer += "type_object->getStepParameter( stream, false );\n";
							
					entity_writer += "}\n";
					entity_writer += "else\n";
					entity_writer += "{\n";
					entity_writer += "stream << \"$\";\n";
					entity_writer += "}\n";
					entity_writer += "}\n";
					entity_writer += "}\n";
					entity_writer += "stream << \")\"; \n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
			{
				if( att_type->m_type->m_super_primitive_CPP.compare( "double" ) == 0 )
				{
					entity_writer += "writeNumericTypeList3D( stream, m_" + name + " );\n";
				}
				else if( att_type->m_type->m_super_primitive_CPP.compare( "int" ) == 0 )
				{
					entity_writer += "writeNumericTypeList3D( stream, m_" + name + " );\n";
				}
				else
				{
					if( att_type->m_type->m_select.size() > 0 )
					{
						entity_writer += "writeTypeList3D( stream, m_" + name + ", true );\n";
					}
					else
					{
						entity_writer += "writeTypeList3D( stream, m_" + name + " );\n";
					}
				}
			}
		}
		else if( dynamic_pointer_cast<AttributeEntity>(att) )
		{
			shared_ptr<AttributeEntity>  att_entity = dynamic_pointer_cast<AttributeEntity>(att);
			if( att->m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				// single entity attribute
				shared_ptr<Entity> att_entity_entity( att_entity->m_entity );
				if( m_generator->m_map_entities.find(att_entity_entity->m_entity_name) == m_generator->m_map_entities.end() )
				{
					m_generator->slotEmitTxtOutWarning( "err 62" );
				}

				entity_writer += "if( m_" + att->m_name + " ) { stream << \"#\" << m_" + att->m_name + "->m_id; } else { stream << " + undefined_arg + "; }\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
			{
				entity_writer += "writeEntityList( stream, m_" + name + " );\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
			{
				entity_writer += "writeEntityList2D( stream, m_" + name + " );\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
			{
				entity_writer += "writeEntityList3D( stream, m_" + name + " );\n";
			}
		}
		else
		{
			m_generator->slotEmitTxtOutWarning( "err 63" );
		}
		entity_writer += "stream << \",\";\n";
	}
	QString check1 = entity_writer.right( 15 );
	entity_writer = entity_writer.left( entity_writer.length() - 15 );
	QString check2 = entity_writer.right( 15 );

	return entity_writer;
}

QString Entity::getEntityReader( bool lowermost, int& iarg )
{
	const QString IfcPP = m_generator->getFilenamePrefix();
	QString entity_reader;

	if( lowermost )
	{
		if( iarg != 0 )
		{
			m_generator->slotEmitTxtOutWarning( "lowermost & iarg!= 0" );
		}
		// use default constructor
		int num_all_attributes = getNumAttributesIncludingSupertypes();

		if( num_all_attributes > 0 )
		{
			entity_reader += "const size_t num_args = args.size();\n";
			QString num_str = QString::number( num_all_attributes );
			entity_reader += "if( num_args != " + num_str + " ){ std::stringstream err; err << \"Wrong parameter count for entity " + m_entity_name + ", expecting " + num_str + ", having \" << num_args << \". Entity ID: \" << m_id << std::endl; throw IfcPPException( err.str().c_str() ); }\n";
		}
	}

	// supertype attributes
	if( m_supertype != 0 )
	{
		entity_reader += m_supertype->getEntityReader( false, iarg );
	}

	// own attributes
	for( int i=0; i<m_attributes.size(); ++i )
	{
		shared_ptr<Attribute>  att = m_attributes[i];
		QString name = att->m_name;
		
		bool use_weak_ptr = false;
		QString datatype = att->getDataTypeWithSmartPointer(use_weak_ptr);

		QString iarg_str = QString::number(iarg);

		if( dynamic_pointer_cast<AttributePrimitive>(att) )
		{
			shared_ptr<AttributePrimitive> att_primitive = dynamic_pointer_cast<AttributePrimitive>(att);
			if( att_primitive->m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				if( att_primitive->m_primitive_type_cpp.compare( "bool" ) == 0 )
				{
					entity_reader += "if( boost::iequals( args["+ iarg_str + "], L\".F.\" ) ) { m_" + name + " = false; }\n";
					entity_reader += "else if( boost::iequals( args["+ iarg_str + "], L\".T.\" ) ) { m_" + name + " = true; }\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "LogicalEnum" ) == 0 )
				{
					entity_reader += "if( boost::iequals( args["+ iarg_str + "], L\".F.\" ) ) { m_" + name + " = LOGICAL_FALSE; }\n";
					entity_reader += "else if( boost::iequals( args["+ iarg_str + "], L\".T.\" ) ) { m_" + name + " = LOGICAL_TRUE; }\n";
					entity_reader += "else if( boost::iequals( args["+ iarg_str + "], L\".U.\" ) ) { m_" + name + " = LOGICAL_UNKNOWN; }\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "int" ) == 0 )
				{
					entity_reader += "readIntValue( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "double" ) == 0 )
				{
					entity_reader += "readRealValue( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "std::wstring" ) == 0 )
				{
					entity_reader += "m_" + name + " = args[" + iarg_str + "];\n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR || att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D || att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
			{
				QString vec_dim = "";
				if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
				{
					vec_dim = "2D";
				}
				else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
				{
					vec_dim = "3D";
				}

				// vector of 
				if( att_primitive->m_primitive_type_cpp.compare( "bool" ) == 0 )
				{
					entity_reader += "readBoolList" + vec_dim + "(  args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "LogicalEnum" ) == 0 )
				{
					entity_reader += "readLogicalList" + vec_dim + "(  args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "int" ) == 0 )
				{
					entity_reader += "readIntList" + vec_dim + "(  args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "double" ) == 0 )
				{
					entity_reader += "readRealList" + vec_dim + "( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "const wchar_t*" ) == 0 )
				{
					entity_reader += "readBinaryList" + vec_dim + "( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_primitive->m_primitive_type_cpp.compare( "std::wstring" ) == 0 )
				{
					entity_reader += "readStringList" + vec_dim + "( args[" + iarg_str + "], m_" + name + " );\n";
				}
			}
		}
		else if( dynamic_pointer_cast<AttributeType>(att) )
		{
			shared_ptr<AttributeType>  att_type = dynamic_pointer_cast<AttributeType>(att);

			if( att->m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				QString reader_args = "args[" + iarg_str + "]";
				if( att_type->m_type->m_select.size() > 0 || m_generator->m_always_put_map_in_type_reader )
				{
					reader_args = "args[" + iarg_str + "], map";
				}
				entity_reader += "m_" + name +" = " + att_type->m_type->m_className + "::createObjectFromSTEP( " + reader_args + " );\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
			{
				if( att_type->m_type->m_super_primitive_CPP.compare( "double" ) == 0 )
				{
					entity_reader += "readTypeOfRealList( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_type->m_type->m_super_primitive_CPP.compare( "int" ) == 0 )
				{
					entity_reader += "readTypeOfIntList( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_type->m_type->m_super_primitive_CPP.compare( "std::wstring" ) == 0 )
				{
					entity_reader += "readTypeOfStringList( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_type->m_type->m_select.size() > 0 )
				{
					entity_reader += "readSelectList( args[" + iarg_str + "], m_" + name + ", map );\n";
				}
				else
				{
					entity_reader += "readTypeList( args[" + iarg_str + "], m_" + name + ", map );\n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
			{
				if( att_type->m_type->m_super_primitive_CPP.compare( "double" ) == 0 )
				{
					entity_reader += "readTypeOfRealList2D( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_type->m_type->m_super_primitive_CPP.compare( "int" ) == 0 )
				{
					entity_reader += "readTypeOfIntList2D( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else
				{
					entity_reader += "readEntityReferenceList2D( args[" + iarg_str + "], m_" + name + ", map );\n";
				}
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
			{
				if( att_type->m_type->m_super_primitive_CPP.compare( "double" ) == 0 )
				{
					entity_reader += "readTypeOfRealList3D( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else if( att_type->m_type->m_super_primitive_CPP.compare( "int" ) == 0 )
				{
					entity_reader += "readTypeOfIntList3D( args[" + iarg_str + "], m_" + name + " );\n";
				}
				else
				{
					entity_reader += "readEntityReferenceList3D( args[" + iarg_str + "], m_" + name + ", map );\n";
				}
			}
		}
		else if( dynamic_pointer_cast<AttributeEntity>(att) )
		{

			shared_ptr<AttributeEntity>  att_entity = dynamic_pointer_cast<AttributeEntity>(att);
			if( att->m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				// single entity attribute
				shared_ptr<Entity> att_entity_entity( att_entity->m_entity );
				if( m_generator->m_map_entities.find(att_entity_entity->m_entity_name) == m_generator->m_map_entities.end() )
				{
					m_generator->slotEmitTxtOutWarning( "err 72" );
				}

				entity_reader += "readEntityReference( args[" + iarg_str + "], m_" + name + ", map );\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR )
			{
				entity_reader += "readEntityReferenceList( args[" + iarg_str + "], m_" + name + ", map );\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_2D )
			{
				entity_reader += "readEntityReferenceList2D( args[" + iarg_str + "], m_" + name + ", map );\n";
			}
			else if( att->m_cardinality == Attribute::CARDINALITY_VECTOR_3D )
			{
				entity_reader += "readEntityReferenceList3D( args[" + iarg_str + "], m_" + name + ", map );\n";
			}
		}
		else
		{
			m_generator->slotEmitTxtOutWarning( "err 73" );
		}
		++iarg;
	}
	return entity_reader;
}
