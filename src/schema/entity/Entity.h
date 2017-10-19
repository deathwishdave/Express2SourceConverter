/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <vector>

class Attribute;
class ExpressToSourceConverter;
class TypeEntityContainer;
class InverseCounterpart;

class Entity
{
public:
	Entity( ExpressToSourceConverter* generator );
	~Entity();

	QString getSuperTypeName() { return m_EXPRESS_supertype_name; }
	shared_ptr<Entity>  getSuperType() { return m_supertype; }
	int getNumAttributesIncludingSupertypes();

	QString getCodeHeader();
	QString getCodeCpp();
	void getCodeGetAttributes( QString& code_get_attributes, QString& code_get_attributes_inverse );
	void getCodeResetAttributes( QString& code_rest_attributes, QString& code_reset_attributes_inverse );
	QString getCodeDeepCopy();
	QString getCodeWriterEntity( bool called_from_subtype );
	QString getAttributeDeclaration( int num_tabs, bool as_comment = false );
	
	bool setSchema( QString schema );
	void setSchemaVersion( QString schema_version );
	QString getSchemaVersion() { return m_schema_version; }
	void parseEntity();
	void parseInverseAttributes( QString inv_code );
	QString getEntityReader( bool lowermost, int& iarg );
	int getStrlenAttributesTypes();
	int getStrlenAttributesTypesAndNames();
	Attribute::Cardinality getCardinalityOfAttributeName( QString attribute_name );
	shared_ptr<Attribute> getAttributeWithName( QString attribute_name );
	
	void getAllSuperClasses( QStringList& all_super_classes );
	void getAllAttributesNames( QStringList& set_all_attributes, bool include_inherited );
	void getAllAttributes( std::vector<shared_ptr<Attribute> >& attributes, bool include_inherited );
	void linkDependencies();
	bool hasBoostOptional();

	ExpressToSourceConverter*		m_generator;
	QString							m_schema_version;
	QString							m_entity_name;
	QString 						m_schema;
	QString 						m_EXPRESS_supertype_name;

	shared_ptr<Entity>  			m_supertype;
	QStringList						m_select_supertypes;
	std::vector<shared_ptr<Attribute> > 		m_attributes;
	std::vector<shared_ptr<AttributeInverse> > 	m_inverse_attributes;
	std::vector<shared_ptr<InverseCounterpart> > 	m_inverse_counterparts;
	
};
