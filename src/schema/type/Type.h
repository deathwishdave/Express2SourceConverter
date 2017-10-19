/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#pragma once

#include <set>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "schema/entity/Attribute.h"
class ExpressToSourceConverter;

class Type
{
public:
	Type( ExpressToSourceConverter* gen );
	~Type();

	int getNumAttributesIncludingSupertypes();
	QString getCodeHeader();
	QString getCodeWriterType();
	QString getCodeTypeToString();

	bool setSchema( QString schema );
	void setSchemaVersion( QString schema_version );

	void parseType();
	QString getTypeReader( QString obj_name );
	QString getCodeCpp();
	QString getCodeDeepCopy();
	void getAllSuperClasses( QStringList& all_super_classes, bool ifcpp_type = true );
	void getAllSelectClasses( QStringList& all_select_classes );
	void findFirstStringSupertype( shared_ptr<Type>& string_super_type );
	void linkDependencies();

	ExpressToSourceConverter*	m_generator;
	QString					m_className;
	QString 				m_schema;
	
	QString 				m_header;
	QString					m_base_datatype_EXPRESS;
	QString 				m_super_primitive_CPP;

	QString					m_attribute_declaration;
	int						m_supertype_cardinality_min;
	int						m_supertype_cardinality_max;
	shared_ptr<Type> 		m_super_type;
	shared_ptr<Entity> 		m_super_entity;
	Attribute::Cardinality	m_super_cardinality;
	QStringList 			m_enums;
	QStringList 			m_select;
	QStringList				m_select_supertypes;
	bool					m_reader_needs_object_map;
	QString					m_schema_version;
};
