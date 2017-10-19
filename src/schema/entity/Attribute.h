/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#pragma once

#include <QtCore/QString>
#include "schema/shared_ptr.h"

class Type;
class Entity;

class Attribute
{
public:
	enum Cardinality
	{
		CARDINALITY_UNDEFINED,
		CARDINALITY_SINGLE,
		CARDINALITY_VECTOR,
		CARDINALITY_VECTOR_2D,
		CARDINALITY_VECTOR_3D
	};
	Attribute( QString name, QString comment = "" )
	{
		m_name			= name;
		m_comment		= comment;
		m_cardinality	= CARDINALITY_UNDEFINED;
		m_optional		= false;
	}
	virtual ~Attribute(){};
	virtual QString getDataType() = 0;
	virtual QString getDataTypeWithSmartPointer(bool weak = false) = 0;
	virtual bool hasBoostOptional() { return false; }
	QString				m_name;
	QString				m_comment;
	Cardinality			m_cardinality;
	bool				m_optional;
};

class AttributePrimitive : public Attribute
{
public:
	AttributePrimitive( QString type_EXPRESS, QString type_cpp, QString name, QString comment = "" ) : Attribute( name, comment )
	{
		m_primitive_type_EXPRESS = type_EXPRESS;
		m_primitive_type_cpp = type_cpp;
	}
	virtual QString getDataType() { return m_primitive_type_cpp; }
	virtual bool hasBoostOptional()
	{
		if( m_optional )
		{
			if( m_cardinality == Attribute::CARDINALITY_SINGLE )
			{
				return true;
			}
		}
		return false;
	}
	virtual QString getDataTypeWithSmartPointer(bool weak = false)
	{
		// TODO:  all optional doubles in IfcEntities. There are only 5 or 6. shared_ptr<double> or boost::optional<double>.
		if( hasBoostOptional() )
		{
			return "boost::optional<" + m_primitive_type_cpp + ">";
		}
		return m_primitive_type_cpp;
	}
	QString		m_primitive_type_EXPRESS;
	QString		m_primitive_type_cpp;
};

class AttributeType : public Attribute
{
public:
	AttributeType( shared_ptr<Type>  type, QString name, QString comment = "" ) : Attribute( name, comment )
	{
		m_type = type;
	}
	virtual QString getDataType();
	virtual QString getDataTypeWithSmartPointer(bool weak = false);
	shared_ptr<Type> 				m_type;
};

class AttributeEntity : public Attribute
{
public:
	
	AttributeEntity( weak_ptr<Entity> ent, QString name, QString comment = "" ) : Attribute( name, comment )
	{
		m_entity			= ent;
	}
	virtual QString getDataType();
	virtual QString getDataTypeWithSmartPointer(bool weak = false);
	weak_ptr<Entity> 				m_entity;
};

class AttributeInverse : public AttributeEntity
{
public:

	AttributeInverse( weak_ptr<Entity> ent, QString name, QString for_counterpart, QString comment = "" ) : AttributeEntity( ent, name, comment )
	{
		m_for_counterpart	= for_counterpart;
	}
	QString m_for_counterpart;
};

class InverseCounterpart
{
public:
	QString m_counterpart_class;
	QString m_counterpart_name;
	QString m_counterpart_type;
	QString m_for;
	Attribute::Cardinality m_counterpart_cardinality;
};
