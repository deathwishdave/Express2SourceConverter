/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#include <sstream>
#include <string>
#include "schema/ExpressToSourceConverter.h"
#include "schema/entity/Attribute.h"
#include "schema/entity/Entity.h"
#include "Type.h"

Type::Type( ExpressToSourceConverter* generator )
{
	m_generator = generator;
	m_className = "";
	m_schema = "";
	m_header = "";

	m_super_primitive_CPP = "";
	m_supertype_cardinality_min = 1;
	m_supertype_cardinality_max = 1;
	m_super_type;

	m_reader_needs_object_map = false;
}

Type::~Type(){}

bool Type::setSchema( QString schema )
{
	m_schema = schema;
	return true;
}

void Type::setSchemaVersion( QString schema_version )
{
	m_schema_version = schema_version;
}

void Type::parseType()
{
	// extract header
	int pos_header_end;
	QRegExp rx_header_end( ";" );
	pos_header_end = rx_header_end.indexIn( m_schema, 0 );

	if( pos_header_end > 0 )
	{
		QStringRef header_str_ref( &m_schema, 0, pos_header_end+rx_header_end.matchedLength() );
		QString header_str = header_str_ref.toString();
		m_header = header_str;

		int pos_supertype = 0;
		QRegExp rx_supertype( "=[\\s]*([a-zA-Z0-9_\\s\\(\\)\\[\\]\\:,\\-\"\\?]*);" );
		if( (pos_supertype = rx_supertype.indexIn( header_str, 0 ) ) != -1 )
		{
			QString complete_match = rx_supertype.cap(0);
			QString supertype = rx_supertype.cap(1);
			supertype.remove( QRegExp( "^\\s*" ) );
			supertype.remove( QRegExp( "\\s*$" ) );
			m_base_datatype_EXPRESS = supertype;
		}
		else
		{
			m_generator->slotEmitTxtOutWarning( "Type: could not find supertype in header: " + header_str );	
		}
	}
	else
	{
		m_generator->slotEmitTxtOutWarning( "Type: could not detect header end: " + m_schema );	
	}

	m_attribute_declaration = "";
	if( m_generator->m_basic_types.find( m_base_datatype_EXPRESS ) != m_generator->m_basic_types.end() )
	{
		m_super_primitive_CPP = m_generator->m_basic_types[m_base_datatype_EXPRESS]->m_CPP_typename;
		m_supertype_cardinality_min = 1;
		m_supertype_cardinality_max = 1;
		m_attribute_declaration = m_super_primitive_CPP + " m_value;\n";

		QRegExp rx_type_begin( "TYPE\\s+([a-zA-Z0-9_]+)\\s*=\\s*([a-zA-Z0-9_\\s\\(\\)\\[\\]\\:,\\-\"]+)\\s*;\\s*END_TYPE;" );

		if( rx_type_begin.indexIn( m_schema, 0 ) != -1 )
		{
			QString complete_match = rx_type_begin.cap(0);
			
			// contains no where statements
			//TYPE IfcAreaMeasure = REAL;
			//END_TYPE;
		}

	}
	else if( m_base_datatype_EXPRESS.startsWith(  "ARRAY" ) || m_base_datatype_EXPRESS.startsWith( "LIST" ) || m_base_datatype_EXPRESS.startsWith( "SET" ) )
	{
		//ARRAY [1:4] OF REAL						-> ifc data: (5182.87,487.0,635272.05756)
		//LIST	[3:3] OF INTEGER					-> ifc data: (5182,487,5)
		//SET	[1:?] OF IfcRepresentationContext	-> ifc data: (#11,#33)


		// TYPE IfcComplexNumber = ARRAY [1:2] OF REAL;
		//typedef std::vector<double> IfcComplexNumber;// = ARRAY[1:2] OF REAL;
		//IfcComplexNumber m_complex_number;
		//m_complex_number.push_back( 2.2 );


		//typedef std::vector<shared_ptr<AttributePrimitive> > IfcRepresentationContext;// = ARRAY[1:2] OF REAL;
		//IfcRepresentationContext m_vec_test;
		//m_vec_test.push_back( shared_ptr<AttributePrimitive>( new AttributePrimitive( "test_EXPRESS", "test", "test" ) ) );


		int pos = 0;
		QRegExp rx( "(ARRAY|LIST|SET)\\s\\[([0-9]+):([0-9\\?]+)\\]\\sOF\\s([a-zA-Z0-9_]+)" );
		if( (pos = rx.indexIn( m_base_datatype_EXPRESS, 0 ) ) == -1 )
		{
			m_generator->slotEmitTxtOut( "couln't understand ARRAY/LIST/SET supertype: " + m_base_datatype_EXPRESS );
			return;
		}
		QString complete_match = rx.cap(0);
		QString container_type = rx.cap(1);
		QString min = rx.cap(2);
		QString max = rx.cap(3);
		QString datatype = rx.cap(4);

		if( min.compare( "?" ) == 0 )
		{
			m_supertype_cardinality_min = -1;
		}
		else
		{
			m_supertype_cardinality_min = min.toInt();
		}
		
		if( max.compare( "?" ) == 0 )
		{
			m_supertype_cardinality_max = -1;
		}
		else
		{
			m_supertype_cardinality_max = max.toInt();
		}
		// TODO: generate cardinality checking

		m_super_cardinality = Attribute::CARDINALITY_VECTOR;
		// TODO: handle multi-dimensional vectors
		if( m_generator->m_basic_types.find( datatype ) != m_generator->m_basic_types.end() )
		{
			m_super_primitive_CPP = m_generator->m_basic_types[datatype]->m_CPP_typename;
			m_attribute_declaration = "std::vector<" + m_super_primitive_CPP +"> m_vec;\n";
		}
		else if( m_generator->m_map_types.find( datatype ) != m_generator->m_map_types.end() )
		{
			m_super_type = m_generator->m_map_types[datatype];
			m_attribute_declaration = "std::vector<" + datatype +"> m_vec;\n";
		}
		else if( m_generator->m_map_entities.find( datatype ) != m_generator->m_map_entities.end() )
		{
			m_super_entity = m_generator->m_map_entities[datatype];
			m_attribute_declaration = "std::vector<shared_ptr<" + datatype +"> > m_vec;\n";
			m_reader_needs_object_map = true;
		}
		else
		{
			m_generator->slotEmitTxtOutWarning( "Type: unhandled array supertype: " + datatype );	
		}
	}
	else if( m_base_datatype_EXPRESS.startsWith( "STRING" ) )
	{
		// STRING(22) FIXED
		m_base_datatype_EXPRESS = "STRING";
		m_super_primitive_CPP = "std::wstring";
		m_supertype_cardinality_min = 1;
		m_supertype_cardinality_max = 1;
		m_attribute_declaration = "std::wstring m_value;\n";

	}
	else if( m_base_datatype_EXPRESS.startsWith( "ENUMERATION" ) )
	{
		// ENUMERATION OF (AHEAD ,BEHIND);
		int pos = 0;
		QRegExp rx( "ENUMERATION\\s*OF\\s*\\(([a-zA-Z0-9_,\\-\\r\\n\\s\"]+)\\)" );
		if( (pos = rx.indexIn( m_base_datatype_EXPRESS, 0 ) ) == -1 )
		{
			m_generator->slotEmitTxtOutWarning( "couln't understand ENUMERATION supertype: " + m_base_datatype_EXPRESS );
			return;
		}
		QString complete_match = rx.cap(0);
		QString enum_values = rx.cap(1);
		enum_values.remove( QRegExp( "[\\r\\n\\s]" ) );
		m_enums = enum_values.split( "," ); 
		m_attribute_declaration = m_className + "Enum m_enum;\n";
	}
	else if( m_base_datatype_EXPRESS.startsWith( "SELECT" ) )
	{
		//SELECT	(IfcRatioMeasure	,IfcMeasureWithUnit	,IfcMonetaryMeasure);
		int pos = 0;
		QRegExp rx( "SELECT\\s*\\(([a-zA-Z0-9_,\\r\\n\\s]+)\\)" );
		if( (pos = rx.indexIn( m_base_datatype_EXPRESS, 0 ) ) == -1 )
		{
			m_generator->slotEmitTxtOutWarning( "couln't understand SELECT supertype: " + m_base_datatype_EXPRESS );
			return;
		}
		QString complete_match = rx.cap(0);
		QString select_values = rx.cap(1);
		select_values.remove( QRegExp( "[\\r\\n\\s]" ) );
		m_select = select_values.split( "," );

		m_reader_needs_object_map = true;
	}
	else if( m_generator->m_map_types.find( m_base_datatype_EXPRESS ) != m_generator->m_map_types.end() )
	{
		m_super_type = m_generator->m_map_types[m_base_datatype_EXPRESS];
	}
	else 
	{
		m_generator->slotEmitTxtOutWarning( "unknown base type: " + m_base_datatype_EXPRESS );
		return;
	}
	return;
}

void Type::getAllSuperClasses( QStringList& set, bool ifcpp_type )
{
	const QString IfcPP = m_generator->getFilenamePrefix();
	if( m_super_type )
	{
		set.append( m_super_type->m_className );
		m_super_type->getAllSuperClasses( set );
	}

	if( m_generator->SELECT_DERIVE )
	{
		for( int j=0; j<m_select_supertypes.size(); ++j )
		{
			QString sel = m_select_supertypes.at( j );
			if( m_generator->m_map_types.find(sel)!=m_generator->m_map_types.end() )
			{
				shared_ptr<Type>  t = m_generator->m_map_types[sel];
				set.append(t->m_className);
				t->getAllSuperClasses(set);
			}
			if( m_generator->m_map_entities.find(sel)!=m_generator->m_map_entities.end() )
			{
				shared_ptr<Entity>  e = m_generator->m_map_entities[sel];
				set.append(e->m_entity_name);
				e->getAllSuperClasses(set);
			}
		}
	}

	if( !m_super_type )
	{
		if( ifcpp_type )
		{
			set.append( "IfcPPObject" );
		}
	}
	set.removeDuplicates();
}

void Type::getAllSelectClasses( QStringList& set )
{
	foreach( QString sel, m_select )
	{
		if( m_generator->m_map_types.find(sel)!=m_generator->m_map_types.end() )
		{
			shared_ptr<Type>  t = m_generator->m_map_types[sel];
			set.append(t->m_className);
			t->getAllSelectClasses(set);
		}
		if( m_generator->m_map_entities.find(sel)!=m_generator->m_map_entities.end() )
		{
			shared_ptr<Entity>  e = m_generator->m_map_entities[sel];
			set.append(e->m_entity_name);
//			e->getAllSelectClasses(set);
		}
	}
}

void Type::linkDependencies()
{
	for( int j=0; j<m_select.size(); ++j )
	{
		QString sel = m_select.at( j );
		if( m_generator->m_map_types.find(sel)!=m_generator->m_map_types.end() )
		{
			shared_ptr<Type>  t = m_generator->m_map_types[sel];
			t->m_select_supertypes.append(m_className);
		}
		if( m_generator->m_map_entities.find(sel)!=m_generator->m_map_entities.end() )
		{
			shared_ptr<Entity>  e = m_generator->m_map_entities[sel];
			e->m_select_supertypes.append(m_className);
		}
	}
}

QString Type::getCodeHeader()
{
	const QString filename_prefix = m_generator->getFilenamePrefix();
	QString namesp = "";
	if( m_generator->m_schema_namespace.length() > 0 )
	{
		namesp = m_generator->m_schema_namespace + "::";
	}
	QString add_ifcpp = m_generator->m_add_ifcpp_in_path ? "ifcpp/" : "";

	m_select_supertypes.removeDuplicates();
	QStringList set_select_superclasses;
	QString type_inherit;
	QString additional_includes;

	if( m_select_supertypes.size() > 0 )
	{
		QStringList direct_base_classes;

		if( m_super_type )
		{
			direct_base_classes.append(m_super_type->m_className);
		}

		direct_base_classes.append(m_select_supertypes);
		getAllSuperClasses(set_select_superclasses);

		QStringList derived_from;
		foreach( QString sel, direct_base_classes)
		{
			// check if select base type is already base of other base type
			bool already_other_base = false;
			foreach( QString other_base, set_select_superclasses)
			{
				if( other_base.compare(sel)==0 )
				{
					continue;
				}

				QStringList base_classes_of_other_base;
				if( m_generator->m_map_types.find(other_base)!=m_generator->m_map_types.end())
				{
					shared_ptr<Type>  t = m_generator->m_map_types[other_base];

					t->getAllSuperClasses(base_classes_of_other_base);
				}
				else if( m_generator->m_map_entities.find(other_base)!=m_generator->m_map_entities.end())
				{
					shared_ptr<Entity>  e = m_generator->m_map_entities[other_base];
					e->getAllSuperClasses(base_classes_of_other_base);
				}

				if( base_classes_of_other_base.contains(other_base) )
				{
					already_other_base = true;
				}
			}
			
			if( already_other_base )
			{
				continue;
			}

			if( !derived_from.contains(sel) )
			{
				if( type_inherit.size() > 0 )
				{
					type_inherit += ", public ";
				}
				else
				{
					type_inherit += " : public ";
				}
				type_inherit += sel;

				if( m_generator->m_map_types.find(sel)!=m_generator->m_map_types.end())
				{
					shared_ptr<Type>  t = m_generator->m_map_types[sel];
					additional_includes += "#include \"" + t->m_className + ".h\"\n";
					t->getAllSuperClasses(derived_from);
				}
				else if( m_generator->m_map_entities.find(sel)!=m_generator->m_map_entities.end())
				{
					shared_ptr<Entity>  e = m_generator->m_map_entities[sel];

					additional_includes += "#include \"" + e->m_entity_name + ".h\"\n";
					e->getAllSuperClasses(derived_from);
				}
			}
		}
	}
	else
	{
		if( m_super_type )
		{
			additional_includes += "#include \"" + m_super_type->m_className + ".h\"\n";

			QString derive_virtual = "";
			if( m_super_type->m_select.size() > 0 )
			{
				derive_virtual = "virtual ";
			}

			if( type_inherit.size() > 0 )
			{
				type_inherit += ", " + derive_virtual + "public " + m_super_type->m_className;
			}
			else
			{
				type_inherit += " : " + derive_virtual + "public " + m_super_type->m_className;
			}
		}
	}
	
	QString derive_primitive;
#ifdef DERIVE_PRIMITIVE
	if( m_super_primitive_CPP.size() > 0 )
	{
		QString IfcPP_primitive_type = "";
		if( m_generator->m_basic_to_ifcpp.find( m_super_primitive_CPP ) != m_generator->m_basic_to_ifcpp.end() )
		{
			derive_primitive = filename_prefix + m_generator->m_basic_to_ifcpp[m_super_primitive_CPP];
		}
		else
		{
			m_generator->slotEmitTxtOutWarning( "unknown supertype: " + m_super_primitive_CPP );
		}


		if( type_inherit.size() > 0 )
		{
			type_inherit += ", public " + derive_primitive;
		}
		else
		{
			type_inherit += " : public " + derive_primitive;
		}
	}
#endif

	if( type_inherit.size() == 0 )
	{
		if( m_select.size() > 0)
		{
			type_inherit = " : virtual public IfcPPObject";//"AbstractSelect";
		}
		else if( m_enums.size() > 0 )
		{
			type_inherit = " : virtual public IfcPPObject";//AbstractEnum";
		}
		else
		{
			type_inherit = " : public IfcPPObject";//"Type";
		}
	}

	QString reader_args = "const std::wstring& arg";

	if( m_reader_needs_object_map || m_generator->m_always_put_map_in_type_reader )
	{
		reader_args = "const std::wstring& arg, const std::map<int,shared_ptr<IfcPPEntity> >& map";
	}
	
	if( m_select.size() > 0 )
	{
		// abstract class
		QString code_h( m_generator->m_header_comment );
		code_h += "\n#pragma once\n";
		code_h += "#include <iostream>\n";
		code_h += "#include <sstream>\n";
		code_h += "#include <map>\n";
		code_h += "#include \"" + add_ifcpp + "model/IfcPPBasicTypes.h\"\n";
		code_h += "#include \"" + add_ifcpp + "model/IfcPPObject.h\"\n";

		code_h += additional_includes;
		code_h += "\n";
		
		if( m_generator->m_schema_namespace.length() > 0 )
		{
			code_h += "namespace " + m_generator->m_schema_namespace + "\n{\n";
		}
		code_h += "// " + m_header.replace( '\n', "" ) + "\n";
		code_h += "class IFCPP_EXPORT " + m_className + type_inherit + "\n{\n";
		code_h += "public:\n";
		code_h += "virtual shared_ptr<IfcPPObject> getDeepCopy( IfcPPCopyOptions& options ) = 0;\n";
		code_h += "virtual void getStepParameter( std::stringstream& stream, bool is_select_type = false ) const = 0;\n";
		code_h += "virtual const std::wstring toString() const = 0;\n";
		code_h += "static shared_ptr<" + namesp + m_className + "> createObjectFromSTEP( " + reader_args + " );\n";

		code_h += "};\n";
		if( m_generator->m_schema_namespace.length() > 0 )
		{
			code_h += "}\n";
		}
		code_h += "\n";
		
		return code_h;
	}

	// class is not abstract. if not yet derived from IfcPPObject, derive from it
	if( !type_inherit.contains( "IfcPPObject" ) )
	{
		QStringList set_all_superclasses;
		getAllSuperClasses( set_all_superclasses, false );

		if( !set_all_superclasses.contains( "IfcPPObject" ) )
		{
			if( derive_primitive.size() == 0 )
			{
				if( type_inherit.size() > 0 )
				{
					type_inherit += ", public IfcPPObject";
				}
				else
				{
					type_inherit += " : public IfcPPObject";
				}
			}
		}
	}

	// enums
	QString enums;
	if( m_enums.size() > 0 )
	{
		enums += "enum " + m_className + "Enum\n";
		enums += "{\n";
		for( int i=0; i<m_enums.size(); ++i )
		{
			if( i>0 )
			{
				enums		+= ",\n";
			}
			QString en = m_enums[i];
			if( en.compare( "NULL" ) == 0 )
			{
				// cpp doesn't accept NULL as enum value
				en = "ENUM_NULL";
			}
			enums += "ENUM_" + en;
		}
		enums += "};\n\n";
	}

	QString code_h( m_generator->m_header_comment );
	code_h += "\n#pragma once\n";
	code_h += "#include <vector>\n";
	code_h += "#include <map>\n";
	code_h += "#include <sstream>\n";
	code_h += "#include <string>\n";
	code_h += "#include \"" + add_ifcpp + "model/IfcPPBasicTypes.h\"\n";
	code_h += "#include \"" + add_ifcpp + "model/IfcPPObject.h\"\n";
	code_h += "#include \"" + add_ifcpp + "model/IfcPPGlobal.h\"\n";

	code_h += additional_includes;

	if( m_super_entity )
	{
		code_h += "class IFCPP_EXPORT " + m_super_entity->m_entity_name + ";\n";
	}

	code_h += "\n";

	if( m_generator->m_schema_namespace.length() > 0 )
	{
		code_h += "namespace " + m_generator->m_schema_namespace + "\n{\n";
	}

	code_h += "// " + m_header.replace( '\n', "" ) + "\n";
	code_h += "class IFCPP_EXPORT " + m_className + type_inherit + "\n{\n";
	code_h += "public:\n";

	code_h += enums;

	// default constructor
	code_h += m_className + "();\n";

	// supertype primitive constructor
	if( m_generator->m_basic_types.find( m_base_datatype_EXPRESS ) != m_generator->m_basic_types.end() )
	{
		code_h += m_className + "( " + m_super_primitive_CPP + " value );\n";
	}

	// enum constructor
	if( m_enums.size() > 0 )
	{
		code_h += m_className + "( " + m_className + "Enum e ) { m_enum = e; }\n";
	}

	// destructor
	code_h += "~" + m_className + "();\n";

	code_h += "virtual const char* className() const { return \"" + m_className + "\"; }\n";
	code_h += "virtual shared_ptr<IfcPPObject> getDeepCopy( IfcPPCopyOptions& options );\n";

#ifdef COPY_METHOD
	code_h += "virtual void copy( shared_ptr<" + m_className + ">& target );\n";
#endif
	code_h += "virtual void getStepParameter( std::stringstream& stream, bool is_select_type = false ) const;\n";
	code_h += "virtual const std::wstring toString() const;\n";
	code_h += "static shared_ptr<" + m_className + "> createObjectFromSTEP( " + reader_args + " );\n";

	// attributes
	code_h += m_attribute_declaration;

	code_h += "};\n";
	if( m_generator->m_schema_namespace.length() > 0 )
	{
		code_h += "}\n"; // namespace
	}
	code_h += "\n";
	return code_h;
}

QString Type::getCodeCpp()
{
	const QString subfolder_ifc_classes = m_generator->getSubfolderIfcClasses();
	QString add_ifcpp = m_generator->m_add_ifcpp_in_path ? "ifcpp/" : "";
	QString namesp = "";
	if( m_generator->m_schema_namespace.length() > 0 )
	{
		namesp = m_generator->m_schema_namespace + "::";
	}

	QString code_cpp( m_generator->m_header_comment );
	
	if( m_select.size() > 0)
	{
		code_cpp += "#include <map>\n";
		code_cpp += "#include \"" + add_ifcpp + "model/IfcPPBasicTypes.h\"\n";
		code_cpp += "#include \"" + add_ifcpp + "model/IfcPPException.h\"\n";
		code_cpp += "#include \"" + add_ifcpp + "reader/ReaderUtil.h\"\n";
		QStringList set_select_superclasses;

		getAllSelectClasses(set_select_superclasses);
		set_select_superclasses.removeDuplicates();
		bool first_if = true;
		foreach(QString sel, set_select_superclasses)
		{
			// only TYPE obects are inline
			if( m_generator->m_map_types.find(sel) == m_generator->m_map_types.end())
			{
				continue;
			}

			code_cpp += "#include \"" + add_ifcpp + subfolder_ifc_classes + "include/" + sel + ".h\"\n";
		}
	}
	else
	{
		code_cpp += "\n#include <sstream>\n";
		code_cpp += "#include <limits>\n";
		code_cpp += "#include <map>\n";
		code_cpp += "#include \"" + add_ifcpp + "reader/ReaderUtil.h\"\n";
		code_cpp += "#include \"" + add_ifcpp + "writer/WriterUtil.h\"\n";
		code_cpp += "#include \"" + add_ifcpp + "model/IfcPPBasicTypes.h\"\n";
		code_cpp += "#include \"" + add_ifcpp + "model/IfcPPException.h\"\n";
	}

	for( int j=0; j<m_select_supertypes.size(); ++j )
	{
		QString sel = m_select_supertypes.at( j );
		code_cpp += "#include \"" + add_ifcpp + subfolder_ifc_classes + "include/" + sel + ".h\"\n";
	}

	if( m_super_entity )
	{
		code_cpp += "#include \"" + add_ifcpp + subfolder_ifc_classes + "include/" + m_super_entity->m_entity_name + ".h\"\n";
	}

	code_cpp += "#include \"" + add_ifcpp + subfolder_ifc_classes + "include/" + m_className + ".h\"\n";

	// comment with class name
	code_cpp += "\n";
	code_cpp += "// " + m_header.replace( '\n', "" ) + "\n";

	// step writer
	if( m_select.size()> 0)
	{

	}
	else
	{
		// constructors
		code_cpp += namesp + m_className + "::" + m_className + "() {}\n";

		if( m_generator->m_basic_types.find( m_base_datatype_EXPRESS ) != m_generator->m_basic_types.end() )
		{
			code_cpp += namesp + m_className + "::" + m_className + "( " + m_super_primitive_CPP + " value ) { m_value = value; }\n";
		}
		code_cpp += namesp + m_className + "::~" + m_className + "() {}\n";

#ifdef COPY_METHOD
		code_cpp += "void " + namesp + m_className + "::copy( shared_ptr<" + m_className + ">& target )\n";
		code_cpp += "{\n";
		if( m_enums.size() > 0 )
		{
			code_cpp += "target->m_enum = m_enum;\n";
		}
		else
		{
			code_cpp += "target->m_value = m_value;\n";
		}
		code_cpp += "}\n";
#endif

		code_cpp += "shared_ptr<IfcPPObject> " + namesp + m_className + "::getDeepCopy( IfcPPCopyOptions& options )\n";
		code_cpp += "{\n";
		code_cpp += "shared_ptr<" + m_className + "> copy_self( new " + m_className + "() );\n";
		code_cpp += getCodeDeepCopy();
		code_cpp += "return copy_self;\n";
		code_cpp += "}\n";

		code_cpp += "void " + namesp + m_className + "::getStepParameter( std::stringstream& stream, bool is_select_type ) const\n{\n";
		code_cpp += getCodeWriterType();
		code_cpp += "}\n";

		code_cpp += "const std::wstring " + namesp + m_className + "::toString() const\n{\n";
		code_cpp += getCodeTypeToString();
		code_cpp += "}\n";
	}

	QString reader_args = "const std::wstring& arg";
	if( m_reader_needs_object_map || m_generator->m_always_put_map_in_type_reader )
	{
		reader_args = "const std::wstring& arg, const std::map<int,shared_ptr<IfcPPEntity> >& map";
	}

	code_cpp += "shared_ptr<" + namesp + m_className + "> " + namesp + m_className + "::createObjectFromSTEP( " + reader_args + " )\n{\n";		
	code_cpp += getTypeReader("");
	code_cpp += "}\n";

	if( m_select.size() > 0 )
	{

	}
	else
	{
		//code_cpp += "static " + filename_prefix + "TypeFactoryHelper<" + namesp + m_className + "> factory_helper( \"" + m_className.toUpper() + "\" );\n";
	}

	return code_cpp;
}

int Type::getNumAttributesIncludingSupertypes()
{
	int num = 1;
	if( m_super_type )
	{
		num += m_super_type->getNumAttributesIncludingSupertypes();
	}
	
	return num;
}

void Type::findFirstStringSupertype( shared_ptr<Type>& string_super_type )
{
	QStringList all_select_classes;
	getAllSelectClasses( all_select_classes );
	foreach( QString select_class, all_select_classes )
	{
		std::map<QString,shared_ptr<Type> >::iterator it_find_type = m_generator->m_map_types.find( select_class );
		if( it_find_type != m_generator->m_map_types.end() )
		{
			shared_ptr<Type> superclass_select_type = it_find_type->second;
			if( superclass_select_type->m_super_primitive_CPP.compare( "std::wstring" ) == 0 )
			{
				string_super_type = superclass_select_type;
						

				break;
			}
		}
	}
}

QString Type::getTypeReader( QString obj_name )
{
	const QString IfcPP = m_generator->getFilenamePrefix();
	QString type_reader = "";
	QString namesp = "";
	if( m_generator->m_schema_namespace.length() > 0 )
	{
		namesp = m_generator->m_schema_namespace + "::";
	}

	int supertype_count=0;
	if( m_super_type )					++supertype_count;
	if( m_super_entity )				++supertype_count;
	if( m_super_primitive_CPP.size() > 0 )	++supertype_count;
	if( m_enums.size() > 0 )		++supertype_count;
	if( m_select.size() > 0 )		++supertype_count;

	if( supertype_count > 1 )
	{
		m_generator->slotEmitTxtOutWarning( "Type::getTypeParser: more than one supertype detected: " + m_schema );
	}
	if( supertype_count == 0 )
	{
		m_generator->slotEmitTxtOutWarning( "Type::getTypeParser: no supertype detected: " + m_schema );
	}

	QString reader_return;
	if( obj_name.size() == 0 && m_select.size() == 0 )
	{
		// types have only one attribute
		obj_name = "type_object";
		type_reader += "if( arg.compare( L\"$\" ) == 0 ) { return shared_ptr<" + m_className + ">(); }\n";
		type_reader += "else if( arg.compare( L\"*\" ) == 0 ) { return shared_ptr<" + m_className + ">(); }\n";
		type_reader +=  "shared_ptr<" + m_className + "> " + obj_name + "( new " + m_className + "() );\n";
		
		reader_return = "return " + obj_name+ ";\n";
	}

	if( m_super_type )
	{
		type_reader += m_super_type->getTypeReader( obj_name );

	}
	else if( m_super_entity )
	{
		if( m_super_cardinality == Attribute::CARDINALITY_VECTOR )
		{
			type_reader += "readEntityReferenceList(  arg, " + obj_name + "->m_vec, map );\n";
		}
	}
	else if( m_super_primitive_CPP.size() > 0 )
	{
		QString read_type_name = "";
		QString read_list = "";
		QString read_second_arg = "";

		if( m_super_primitive_CPP.compare( "bool" ) == 0 )
		{
			read_type_name = "Bool";
		}
		else if( m_super_primitive_CPP.compare( "LogicalEnum" ) == 0 )
		{
			read_type_name = "Logical";
		}
		else if( m_super_primitive_CPP.compare( "int" ) == 0 )
		{
			read_type_name = "Int";
		}
		else if( m_super_primitive_CPP.compare( "double" ) == 0 )
		{
			read_type_name = "Real";
		}
		else if( m_super_primitive_CPP.compare( "const wchar_t*" ) == 0 )
		{
			read_type_name = "Binary";
		}
		else if( m_super_primitive_CPP.compare( "std::wstring" ) == 0 )
		{
			read_type_name = "String";
		}

		if( m_supertype_cardinality_min == 1 && m_supertype_cardinality_max == 1 )
		{
			read_second_arg = ", " + obj_name + "->m_value";
		}
		else
		{
			read_list = "List";
			read_second_arg = ", " + obj_name + "->m_vec";
		}
		type_reader += "read" + read_type_name + read_list + "( arg" + read_second_arg + " );\n";
	}
	else if( m_enums.size() > 0 )
	{
		for( int j=0; j<m_enums.size(); ++j )
		{
			QString en = m_enums.at( j );
			if( en.compare( "NULL" ) == 0 )
			{
				// cpp doesn't accept NULL as enum value
				en = "ENUM_NULL";
			}

			type_reader += "";
			if( j > 0 )
			{
				type_reader += "else ";
			}
			type_reader += "if( boost::iequals( arg, L\"." + en + ".\" ) )\n";
			type_reader += "{\n";
			type_reader += "" + obj_name + "->m_enum = " + m_className + "::ENUM_" + en + ";\n";
			type_reader += "}\n";
		}
	}
	else if( m_select.size() > 0 )
	{
		type_reader += "if( arg.size() == 0 ){ return shared_ptr<" + m_className+">(); }\n";
		
		type_reader += "if( arg.compare(L\"$\")==0 )\n";
		type_reader += "{\n";
		type_reader += " return shared_ptr<" + m_className+">();\n";
		type_reader += "}\n";
		
		type_reader += "if( arg.compare(L\"*\")==0 )\n";
		type_reader += "{\n";
		type_reader += " return shared_ptr<" + m_className+">();\n";
		type_reader += "}\n";

		type_reader += "shared_ptr<" + m_className+"> result_object;\n";
		type_reader += "readSelectType( arg, result_object, map );\n";
		type_reader += "return result_object;\n";
		return type_reader;
	}
	else
	{
		m_generator->slotEmitTxtOutWarning( "Type: no attributes and no supertype detected: " + m_schema );
	}
	type_reader += reader_return;
	return type_reader;
}

QString getWriterForElementary(QString elementary_type, int min, int max)
{
	QString elementary_writer;
	if( min == 1 && max == 1 )
	{
		if( elementary_type.compare( "bool" ) == 0 )
		{
			elementary_writer += "if( m_value == false )\n";
			elementary_writer += "{\n";
			elementary_writer += "	stream << \".F.\";\n";
			elementary_writer += "}\n";
			elementary_writer += "else if( m_value == true )\n";
			elementary_writer += "{\n";
			elementary_writer += "	stream << \".T.\";\n";
			elementary_writer += "}\n";
		}
		else if( elementary_type.compare( "LogicalEnum" ) == 0 )
		{
			elementary_writer += "if( m_value == LOGICAL_FALSE )\n";
			elementary_writer += "{\n";
			elementary_writer += "	stream << \".F.\";\n";
			elementary_writer += "}\n";
			elementary_writer += "else if( m_value == LOGICAL_TRUE )\n";
			elementary_writer += "{\n";
			elementary_writer += "	stream << \".T.\";\n";
			elementary_writer += "}\n";
			elementary_writer += "else if( m_value == LOGICAL_UNKNOWN )\n";
			elementary_writer += "{\n";
			elementary_writer += "	stream << \".U.\";\n";
			elementary_writer += "}\n";
		}
		else if( elementary_type.compare( "int" ) == 0 )
		{
			elementary_writer += "stream << m_value;\n";
		}
		else if( elementary_type.compare( "double" ) == 0 )
		{
			elementary_writer += "stream << m_value;\n";
		}
		else if( elementary_type.compare( "std::wstring" ) == 0 )
		{
			elementary_writer += "stream << \"\'\" << encodeStepString( m_value ) << \"\'\";\n";
		}
	}
	else
	{
		// vector of 
		if( elementary_type.compare( "bool" ) == 0 )
		{
			elementary_writer += "writeBoolList( stream, m_vec );\n";
		}
		else if( elementary_type.compare( "LogicalEnum" ) == 0 )
		{
			elementary_writer += "writeLogicalList( stream, m_vec );\n";
		}
		else if( elementary_type.compare( "int" ) == 0 )
		{
			elementary_writer += "writeNumericList( stream, m_vec );\n";
		}
		else if( elementary_type.compare( "double" ) == 0 )
		{
			elementary_writer += "writeNumericList( stream, m_vec );\n";
		}
		else if( elementary_type.compare( "const wchar_t*" ) == 0 )
		{
			elementary_writer += "writeConstCharList( stream, m_vec );\n";
		}
		else if( elementary_type.compare( "std::wstring" ) == 0 )
		{
			elementary_writer += "writeStringList( stream, m_vec );\n";
		}
	}
	return elementary_writer;
}

QString getToStringForElementary( QString elementary_type, int min, int max )
{
	QString elementary_writer;
	if( min == 1 && max == 1 )
	{
		if( elementary_type.compare( "bool" ) == 0 )
		{
			elementary_writer += "if( m_value == false )\n";
			elementary_writer += "{\n";
			elementary_writer += "	return L\"false\";\n";
			elementary_writer += "}\n";
			elementary_writer += "else if( m_value == true )\n";
			elementary_writer += "{\n";
			elementary_writer += "	return L\"true\";\n";
			elementary_writer += "}\n";
			elementary_writer += "return L\"\";\n";
		}
		else if( elementary_type.compare( "LogicalEnum" ) == 0 )
		{
			elementary_writer += "if( m_value == LOGICAL_FALSE )\n";
			elementary_writer += "{\n";
			elementary_writer += "	return L\"false\";\n";
			elementary_writer += "}\n";
			elementary_writer += "else if( m_value == LOGICAL_TRUE )\n";
			elementary_writer += "{\n";
			elementary_writer += "	return L\"true\";\n";
			elementary_writer += "}\n";
			elementary_writer += "else if( m_value == LOGICAL_UNKNOWN )\n";
			elementary_writer += "{\n";
			elementary_writer += "	return L\"unknown\";\n";
			elementary_writer += "}\n";
			elementary_writer += "return L\"\";\n";
		}
		else if( elementary_type.compare( "int" ) == 0 || elementary_type.compare( "double" ) == 0 )
		{
			elementary_writer += "std::wstringstream strs;\n\
			strs << m_value;\n\
			return strs.str();\n";
		}
		else if( elementary_type.compare( "std::wstring" ) == 0 )
		{
			elementary_writer += "return m_value;\n";
		}
	}
	else
	{
		// vector of 
		if( elementary_type.compare( "bool" ) == 0 )
		{
			elementary_writer += "std::wstring strs;\n\
				for( size_t ii = 0; ii < m_vec.size(); ++ii )\
				{\
					if( ii > 0 )\
					{\
						result_str.append( L\", \" );\
					}\
					if( m_vec[ii]  )\
					{\
						result_str.append( L\"true\" );\
					}\
					else\
					{\
						result_str.append( L\"false\" );\
					}\
				}\
			return strs; \n";
		}
		else if( elementary_type.compare( "LogicalEnum" ) == 0 )
		{
			elementary_writer += "std::wstring result_str;\n\
				for( size_t ii = 0; ii < m_vec.size(); ++ii )\
				{\
					if( ii > 0 )\
					{\
						result_str.append( L\", \" );\
					}\
					if( m_vec[ii] == LOGICAL_TRUE )\
					{\
						result_str.append( L\"true\" );\
					}\
					else if( m_vec[ii] == LOGICAL_FALSE )\
					{\
						result_str.append( L\"false\" );\
					}\
					else\
					{\
						result_str.append( L\"unknown\" );\
					}\
				}\
			return result_str; \n";
		}
		else if( elementary_type.compare( "int" ) == 0 || elementary_type.compare( "double" ) == 0 )
		{
			elementary_writer += "std::wstringstream strs;\n\
				for( size_t ii = 0; ii < m_vec.size(); ++ii )\
				{\
					if( ii > 0 )\
					{\
						strs << L\", \";\
					}\
					strs << m_vec[ii];\
				}\
			return strs.str(); \n";
		}
		else if( elementary_type.compare( "const wchar_t*" ) == 0 )
		{
			elementary_writer += "std::wstring result_str;\n\
				for( size_t ii = 0; ii < m_vec.size(); ++ii )\
				{\
					if( ii > 0 )\
					{\
						result_str.append( L\", \" );\
					}\
					result_str.append( m_vec[ii] );\
				}\
			return result_str; \n";
		}
		else if( elementary_type.compare( "std::wstring" ) == 0 )
		{
			elementary_writer += "std::wstring result_str;\n\
				for( size_t ii = 0; ii < m_vec.size(); ++ii )\
				{\
					if( ii > 0 )\
					{\
						result_str.append( L\", \" );\
					}\
					result_str.append( m_vec[ii] );\
				}\
			return result_str; \n";
		}
	}
	return elementary_writer;
}

QString Type::getCodeWriterType()
{
	if( m_select.size() > 0 )
	{
		// select classes are abstract and do not implement getStepData
		return "";
	}

	QString code_writer = "";
	code_writer += "if( is_select_type ) { stream << \"" + m_className.toUpper() + "(\"; }\n";
	
	if( m_super_type )
	{
		if( m_super_type->m_super_primitive_CPP.size() > 0 )
		{
			code_writer += getWriterForElementary( m_super_type->m_super_primitive_CPP, m_super_type->m_supertype_cardinality_min, m_super_type->m_supertype_cardinality_max );
		}
		else
		{
			bool writer_done = false;
			if( m_super_type->m_super_type )
			{
				QString className = m_className;
				QString base_type_EXPRESS = m_base_datatype_EXPRESS;

				QString supertype_EXPRESS = m_super_type->m_base_datatype_EXPRESS;
				QString supertype_cpp = m_super_type->m_super_primitive_CPP;

				if( m_super_type->m_super_type->m_super_primitive_CPP.size() > 0 )
				{
					// code_writer += getWriterForElementary( m_super_type->m_super_type->m_super_primitive_CPP, m_supertype_cardinality_min, m_supertype_cardinality_max );
					if( m_supertype_cardinality_max > 1 )
					{
						code_writer += "for( size_t ii = 0; ii < m_vec.size(); ++ii ){\
if( ii > 0 )\
{\
stream << \", \";\
}\
stream << m_vec[ii].m_value;\
}\n";
					}
					writer_done = true;
				}
			}

			if( !writer_done )
			{
				throw std::exception( "unhandled supertype writer" );
			}
		}
	}
	else if( m_super_entity )
	{
		code_writer += "writeEntityList( stream, m_vec );\n";
	}
	else if( m_super_primitive_CPP.size() > 0 )
	{
		code_writer += getWriterForElementary( m_super_primitive_CPP, m_supertype_cardinality_min, m_supertype_cardinality_max );
	}
	else if( m_enums.size() > 0 )
	{
		if( m_enums.size() > 0 )
		{
			code_writer += "switch( m_enum )\n";
			code_writer += "{\n";
			for( int j = 0; j<m_enums.size(); ++j )
			{
				QString en = m_enums.at( j );

				if( en.compare( "NULL" ) == 0 )
				{
					// cpp doesn't accept NULL as enum value
					en = "ENUM_NULL";
				}

				code_writer += "\t";
				if( j > 0 )
				{
					//code_writer += "else ";
				}

				code_writer += "case ENUM_" + en + ":\tstream << \"." + en + ".\"; break;\n";
				
			}
			code_writer += "}\n";
		}
	}
	else
	{
		m_generator->slotEmitTxtOutWarning( "Type: no attributes and no supertype detected: " + m_schema );
	}

	code_writer += "if( is_select_type ) { stream << \")\"; }\n";
	return code_writer;
}

QString Type::getCodeTypeToString()
{
	if( m_select.size() > 0 )
	{
		// select classes are abstract and do not implement getStepData
		return "";
	}

	QString code_writer = "";

	if( m_super_type )
	{
		if( m_super_type->m_super_primitive_CPP.size() > 0 )
		{
			code_writer += getToStringForElementary( m_super_type->m_super_primitive_CPP, m_super_type->m_supertype_cardinality_min, m_super_type->m_supertype_cardinality_max );
		}
		else
		{
			bool writer_done = false;
			if( m_super_type->m_super_type )
			{
				QString className = m_className;
				QString base_type_EXPRESS = m_base_datatype_EXPRESS;

				QString supertype_EXPRESS = m_super_type->m_base_datatype_EXPRESS;
				QString supertype_cpp = m_super_type->m_super_primitive_CPP;

				if( m_super_type->m_super_type->m_super_primitive_CPP.size() > 0 )
				{
					QString super_primitive = m_super_type->m_super_type->m_super_primitive_CPP;
					int min = m_super_type->m_super_type->m_supertype_cardinality_min;
					int max = m_super_type->m_super_type->m_supertype_cardinality_max;
					code_writer += getToStringForElementary( super_primitive, min, max );
					writer_done = true;
				}
			}

			if( !writer_done )
			{
				throw std::exception( "unhandled supertype writer" );
			}
		}
	}
	else if( m_super_entity )
	{
		code_writer += "std::wstring result_str;\n\
				for( size_t ii = 0; ii < m_vec.size(); ++ii )\n\
				{\n\
					if( ii > 0 )\n\
					{\n\
						result_str.append( L\", \" );\n\
					}\n\
					if( m_vec[ii]  )\n\
					{\n\
						result_str.append( m_vec[ii]->toString() );\n\
					}\n\
				}\n\
			return result_str; \n";
	}
	else if( m_super_primitive_CPP.size() > 0 )
	{
		code_writer += getToStringForElementary( m_super_primitive_CPP, m_supertype_cardinality_min, m_supertype_cardinality_max );
	}
	else if( m_enums.size() > 0 )
	{
		if( m_enums.size() > 0 )
		{
			code_writer += "switch( m_enum ) \n";
			code_writer += "{\n";
			for( int j = 0; j<m_enums.size(); ++j )
			{
				QString en = m_enums.at( j );
				QString enum_prefix = "";

				if( en.compare( "NULL" ) == 0 )
				{
					// cpp doesn't accept NULL as enum value
					enum_prefix = "ENUM_";
				}

				code_writer += "\t";
				if( j > 0 )
				{
					//code_writer += "else ";
				}

				code_writer += "case ENUM_" + enum_prefix + en + ":";
				code_writer += "\treturn L\"" + en + "\";\n";
			}
			code_writer += "}\n";
		}
		code_writer += "return L\"\";\n";
	}
	else
	{
		m_generator->slotEmitTxtOutWarning( "Type: no attributes and no supertype detected: " + m_schema );
	}

	return code_writer;
}

QString Type::getCodeDeepCopy()
{
	const QString IfcPP = m_generator->getFilenamePrefix();
	QString type_copy_cpp = "";
	QString namesp = "";
	if( m_generator->m_schema_namespace.length() > 0 )
	{
		namesp = m_generator->m_schema_namespace + "::";
	}

	int supertype_count=0;
	if( m_super_type )					++supertype_count;
	if( m_super_entity )				++supertype_count;
	if( m_super_primitive_CPP.size() > 0 )	++supertype_count;
	if( m_enums.size() > 0 )		++supertype_count;
	if( m_select.size() > 0 )		++supertype_count;

	if( supertype_count > 1 )
	{
		m_generator->slotEmitTxtOutWarning( "Type::getTypeParser: more than one supertype detected: " + m_schema );
	}
	if( supertype_count == 0 )
	{
		m_generator->slotEmitTxtOutWarning( "Type::getTypeParser: no supertype detected: " + m_schema );
	}

	QString reader_return;
	if( m_super_type )
	{
		type_copy_cpp += m_super_type->getCodeDeepCopy();

	}
	else if( m_super_entity )
	{
		if( m_super_cardinality == Attribute::CARDINALITY_VECTOR )
		{
			type_copy_cpp += "for( size_t ii=0; ii<m_vec.size(); ++ii )\n";
			type_copy_cpp += "{\n";
			type_copy_cpp += "auto item_ii = m_vec[ii];\n";
			type_copy_cpp += "if( item_ii )\n{\n";
			type_copy_cpp += "copy_self->m_vec.push_back( dynamic_pointer_cast<" + m_super_entity->m_entity_name + ">( item_ii->getDeepCopy( options ) ) );\n";
			type_copy_cpp += "}\n";
			type_copy_cpp += "}\n";
		}
	}
	else if( m_super_primitive_CPP.size() > 0 )
	{
		if( m_supertype_cardinality_min == 1 && m_supertype_cardinality_max == 1 )
		{
			if( m_super_primitive_CPP.compare( "bool" ) == 0 )
			{
				type_copy_cpp += "copy_self->m_value = m_value;\n";
			}
			else if( m_super_primitive_CPP.compare( "LogicalEnum" ) == 0 )
			{
				type_copy_cpp += "copy_self->m_value = m_value;\n";
			}
			else if( m_super_primitive_CPP.compare( "int" ) == 0 )
			{
				type_copy_cpp += "copy_self->m_value = m_value;\n";
			}
			else if( m_super_primitive_CPP.compare( "double" ) == 0 )
			{
				type_copy_cpp += "copy_self->m_value = m_value;\n";
			}
			else if( m_super_primitive_CPP.compare( "const wchar_t*" ) == 0 )
			{
				type_copy_cpp += "if( m_value != nullptr )\n";
				type_copy_cpp += "{\n";
				type_copy_cpp += "const wchar_t* source_value = m_value;\n";
				type_copy_cpp += "const size_t len = strlen( source_value );\n";
				type_copy_cpp += "wchar_t* target_value = new char[len + 1];\n";
				type_copy_cpp += "strncpy( target_value, source_value, len );\n";
				type_copy_cpp += "target_value[len] = '\\0';  // just to be sure\n";
				type_copy_cpp += "copy_self->m_value = target_value;\n";
				type_copy_cpp += "}\n";
			}
			else if( m_super_primitive_CPP.compare( "std::wstring" ) == 0 )
			{
				type_copy_cpp += "copy_self->m_value = m_value;\n";
			}
			else
			{
				m_generator->slotEmitTxtOutWarning( "Type: unhandled m_super_primitive for copy: " + m_super_primitive_CPP );
			}
		}
		else
		{
			// vector of 

			type_copy_cpp += "for( size_t ii=0; ii<m_vec.size(); ++ii )\n";
			type_copy_cpp += "{\n";
			type_copy_cpp += m_super_primitive_CPP + " item_ii = m_vec[ii];\n";
			type_copy_cpp += "copy_self->m_vec.push_back( item_ii );\n";
			type_copy_cpp += "}\n";
		}
	}
	else if( m_enums.size() > 0 )
	{
		type_copy_cpp += "copy_self->m_enum = m_enum;\n";
	}
	else if( m_select.size() > 0 )
	{
		
	}
	else
	{
		m_generator->slotEmitTxtOutWarning( "Type: no attributes and no supertype detected: " + m_schema );
	}
	type_copy_cpp += reader_return;
	return type_copy_cpp;
}
