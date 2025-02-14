/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2019 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkRTPCKey.h
//
//////////////////////////////////////////////////////////////////////

#ifndef AK_RTPC_KEY
#define AK_RTPC_KEY

#include <AK/Tools/Common/AkKeyArray.h>
#include <AK/SoundEngine/Common/AkMidiTypes.h>
#include "AkMidiStructs.h"

// Declarations

class CAkRegisteredObj;
class CAkPBI;

template <typename T_FIELD, typename T_GET_INVALID> 
class AkKeyCommon;

template <typename T_FIELD, typename T_GET_INVALID, typename T_CHILD_KEY_CLASS> 
class AkNestedKey;

template <typename T_FIELD, typename T_GET_INVALID> 
class AkRootKey;

// ---- Search Tree Types ----

//
// AkRTPCSearchTreeCommon
//

template <typename T_KEY, typename T_VALUE>
class AkRTPCSearchTreeCommon
{
public:
	AkRTPCSearchTreeCommon(): m_bSet(false) {}
	virtual ~AkRTPCSearchTreeCommon() {}
	
	T_VALUE* GetValue() 
	{ 
		return m_bSet ? &m_Value : NULL;
	}

	T_VALUE* SetValue() 
	{ 
		if (!m_bSet)
		{
			AkPlacementNew(&m_Value) T_VALUE();
			m_bSet = true;
		}
		
		return &m_Value;
	}

	void UnsetValue() 
	{ 
		if (m_bSet)
		{
			m_Value.~T_VALUE();
			m_bSet = false;
		}
	}

	T_VALUE& Value(){return m_Value;}

	T_VALUE m_Value;
	bool	m_bSet;
};


//
// AkRTPCNestedSearchTree
//

template <typename T_KEY, typename T_VALUE, typename T_CHILD_TREE> 
class AkRTPCNestedSearchTree: public AkRTPCSearchTreeCommon<T_KEY,T_VALUE>
{
public:
	typedef typename T_KEY::FieldType FieldType;
	typedef typename T_KEY::ChildKeyType ChildKeyType;
	typedef AkRTPCSearchTreeCommon<T_KEY,T_VALUE> BaseClass;

	class ChildTreeType: public T_CHILD_TREE
	{
	public:
		inline void Transfer(ChildTreeType& in_rhs)
		{
			key = in_rhs.key;
			T_CHILD_TREE::Transfer(in_rhs);
		}
		static AkForceInline typename T_KEY::FieldType& Get( ChildTreeType& in_val ) { return in_val.key; }
		typename T_KEY::FieldType key; //For AkKeyArray
	};

	typedef AkSortedKeyArray< FieldType, ChildTreeType, ArrayPoolDefault, ChildTreeType, AkGrowByPolicy_DEFAULT, AkTransferMovePolicy<ChildTreeType> > ChildArrayType;

	typedef bool (*ForEachExFcn) ( T_VALUE& in_value, const T_KEY& pKey, void* in_cookie );
	typedef void (*ForEachFcn) ( T_VALUE& in_value, const T_KEY& pKey, void* in_cookie );

	AkRTPCNestedSearchTree() {}
	virtual ~AkRTPCNestedSearchTree()	{ AKASSERT(mChildren.IsEmpty()); }

	void Transfer( AkRTPCNestedSearchTree<T_KEY,T_VALUE,T_CHILD_TREE>& in_rhs )
	{
		BaseClass::Value() = (in_rhs.BaseClass::Value());
		BaseClass::m_bSet = (in_rhs.BaseClass::m_bSet);
		mChildren.Transfer(in_rhs.mChildren);
	}

	void Term()
	{
		for( typename ChildArrayType::Iterator it = mChildren.Begin(), itEnd = mChildren.End(); it != itEnd; ++it )
		{
			(*it).Term();
		}
		mChildren.Term();
	};

	// Look for a *best possible match*, return exact match if available.  io_matchingKey will be set to the match that was found.
	T_VALUE* FindBestMatch( T_KEY& io_matchingKey )
	{
		T_VALUE* pValue = NULL;

		if ( io_matchingKey.AnyFieldValid() )
		{
			ChildTreeType* pChildTree = mChildren.Exists( io_matchingKey.field );
			if (pChildTree == NULL && io_matchingKey.IsFieldValid())
			{
				io_matchingKey.field = T_KEY::GetInvalidFieldType::Get(); // Invalid field often used to represent "any", id AK_INVALID_GAME_OBJ
				pChildTree = mChildren.Exists( io_matchingKey.field );
			}

			if (pChildTree != NULL)
			{
				pValue = pChildTree->FindBestMatch( io_matchingKey.childKey );
			}
		}

		if (pValue)
		{
			//found a match lower down in the tree.
			return pValue;
		}
		else
		{
			//no matches lower down.
			io_matchingKey.childKey = ChildKeyType(); //clear fields to return matching key.
			return AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		}
	}

	// Look for a *exact match* to the passed in key
	T_VALUE* FindExact( const T_KEY& in_key, T_VALUE** out_ppParentValue = NULL, bool* out_childValuesExist = NULL )
	{
		if ( in_key.AnyFieldValid() )
		{
			if (out_ppParentValue != NULL && BaseClass::m_bSet)
				*out_ppParentValue = &(BaseClass::Value());

			ChildTreeType* pChildTree = mChildren.Exists( in_key.field );
			if (pChildTree != NULL)
				return pChildTree->FindExact( in_key.childKey, out_ppParentValue, out_childValuesExist );
		}
		else
		{
			if (out_childValuesExist != NULL)
				*out_childValuesExist = !mChildren.IsEmpty();
			return AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		}

		return NULL;
	}

	T_VALUE * Set( const T_KEY& in_key )
	{
		if ( in_key.AnyFieldValid() )
		{
			ChildTreeType* pChildTree = mChildren.Set( in_key.field );
			return (pChildTree != NULL) ? pChildTree->Set(in_key.childKey) : NULL;
		}
		else
		{
			return AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::SetValue();
		}
	}

	// Remove a single value specified by in_key, if it exists in the tree.
	void Unset( const T_KEY& in_key )
	{
		if ( in_key.AnyFieldValid() )
		{
			ChildTreeType* pChildTree = mChildren.Exists( in_key.field );
			if (pChildTree != NULL)
			{
				pChildTree->Unset(in_key.childKey);
				if ( pChildTree->IsEmpty() )
				{
					pChildTree->Term();
					mChildren.Unset( in_key.field );
				}
			}
		}
		else
		{
			AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::UnsetValue();
		}
	}

	// Invalid fields in 'in_matchingKey' are considered wildcards.
	void RemoveAll( const T_KEY& in_matchingKey )
	{
		if ( in_matchingKey.AnyFieldValid() )
		{
			if ( !in_matchingKey.IsFieldValid() ) // = "ANY" remove all children
			{
				//Iterate through all
				for( typename ChildArrayType::Iterator it = mChildren.Begin(); it != mChildren.End(); )
				{
					ChildTreeType& childTree = (*it);
					childTree.RemoveAll( in_matchingKey.childKey );
					if ( childTree.IsEmpty() )
					{
						childTree.Term();
						it = mChildren.Erase(it);
					}
					else
					{
						++it;
					}
				}
			}
			else // = Remove specific subtree
			{
				ChildTreeType* pChildTree = mChildren.Exists( in_matchingKey.field );
				if (pChildTree != NULL)
				{
					pChildTree->RemoveAll( in_matchingKey.childKey );
					if ( pChildTree->IsEmpty() )
					{
						pChildTree->Term();
						mChildren.Unset( in_matchingKey.field );
					}
				}
			}
		}
		else
		{
			//No (more) keys are specified.  Unset this value, remove all children and terminate subtree.
			AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::UnsetValue();
			Term();
		}
	}

	AkForceInline bool IsEmpty() const 
	{
		return !AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::m_bSet && mChildren.IsEmpty();
	}

	void ForEach( ForEachFcn in_fcn, void* in_cookie )
	{
		T_KEY key;
		_ForEach( in_fcn, key, key, in_cookie );
	}

	void ForEachEx( ForEachExFcn in_fcn, void* in_cookie )
	{
		T_KEY key;
		_ForEachEx( in_fcn, key, key, in_cookie );
	}

	void ForEachMatching( ForEachFcn in_fcn, const T_KEY& in_matchingKey, void* in_cookie )
	{
		T_KEY key;
		_ForEachMatching( in_fcn, in_matchingKey, key, key, in_cookie, !in_matchingKey.AnyFieldValid() );
	}

	void ForEachMatchingEx( ForEachExFcn in_fcn, const T_KEY& in_matchingKey, void* in_cookie )
	{
		T_KEY key;
		_ForEachMatchingEx( in_fcn, in_matchingKey, key, key, in_cookie, !in_matchingKey.AnyFieldValid() );
	}

	template< typename T_FCN, typename T_TOP_LVL_KEY>
	void _ForEach( T_FCN in_fcn, T_TOP_LVL_KEY& in_curKeyTopLvl, T_KEY& in_currentKey, void* in_cookie )
	{
		T_VALUE* pValue = AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		if (pValue)
			in_fcn( *pValue, in_curKeyTopLvl, in_cookie );

		for( typename ChildArrayType::Iterator it = mChildren.Begin(); it != mChildren.End(); ++it )
		{
			in_currentKey.field = (*it).key;
			(*it).template _ForEach<T_FCN,T_TOP_LVL_KEY>( in_fcn, in_curKeyTopLvl, in_currentKey.childKey, in_cookie );
		}
	}

	template< typename T_FCN, typename T_TOP_LVL_KEY>
	bool _ForEachEx( T_FCN in_fcn, T_TOP_LVL_KEY& in_curKeyTopLvl, T_KEY& in_currentKey, void* in_cookie )
	{
		T_VALUE* pValue = AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		if (pValue)
		{
			if ( in_fcn( *pValue, in_curKeyTopLvl, in_cookie ) )
				AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::UnsetValue();
		}

		for( typename ChildArrayType::Iterator it = mChildren.Begin(); it != mChildren.End(); )
		{
			in_currentKey.field = (*it).key;
			if ( (*it).template _ForEachEx<T_FCN,T_TOP_LVL_KEY>( in_fcn, in_curKeyTopLvl, in_currentKey.childKey, in_cookie ) )
			{
				(*it).Term();
				it = mChildren.Erase(it);
			}
			else
			{
				++it;
			}
		}

		return IsEmpty();
	}

	template< typename T_FCN, typename T_TOP_LVL_KEY>
	inline void _ForEachMatching( T_FCN in_fcn, const T_KEY& in_matchingKey, T_TOP_LVL_KEY& in_curKeyTopLvl, T_KEY& in_currentKey, void* in_cookie, bool in_bCall = true )
	{
		if (in_bCall)
		{
			T_VALUE* pValue = AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
			if (pValue)
				in_fcn( *pValue, in_curKeyTopLvl, in_cookie );
		}

		if ( !in_matchingKey.IsFieldValid() ) //Recurse on all children
		{
			for( typename ChildArrayType::Iterator it = mChildren.Begin(); it != mChildren.End(); ++it)
			{
				in_currentKey.field = (*it).key;
				(*it).template _ForEachMatching<T_FCN,T_TOP_LVL_KEY>( in_fcn, in_matchingKey.childKey, in_curKeyTopLvl, in_currentKey.childKey, in_cookie );
			}
		}
		else // Recurse on a specific child
		{
			ChildTreeType* pChild = mChildren.Exists( in_matchingKey.field );
			if (pChild)
			{
				in_currentKey.field = in_matchingKey.field;
				pChild->template _ForEachMatching<T_FCN,T_TOP_LVL_KEY>( in_fcn, in_matchingKey.childKey, in_curKeyTopLvl, in_currentKey.childKey, in_cookie );
			}
		}
	}

	template< typename T_FCN, typename T_TOP_LVL_KEY>
	bool _ForEachMatchingEx( T_FCN in_fcn, const T_KEY& in_matchingKey, T_TOP_LVL_KEY& in_curKeyTopLvl, T_KEY& in_currentKey, void* in_cookie, bool in_bCall = true )
	{
		if (in_bCall)
		{
			T_VALUE* pValue = AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
			if (pValue)
			{
				if ( in_fcn( *pValue, in_curKeyTopLvl, in_cookie ) )
					AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::UnsetValue();
			}
		}

		if ( !in_matchingKey.IsFieldValid() ) //Recurse on all children
		{
			for( typename ChildArrayType::Iterator it = mChildren.Begin(); it != mChildren.End(); )
			{
				in_currentKey.field = (*it).key;
				if ( (*it).template _ForEachMatchingEx<T_FCN,T_TOP_LVL_KEY>( in_fcn, in_matchingKey.childKey, in_curKeyTopLvl, in_currentKey.childKey, in_cookie ) )
				{
					(*it).Term();
					it = mChildren.Erase(it);
				}
				else
				{
					++it;
				}
			}
		}
		else // Recurse on a specific child
		{
			ChildTreeType* pChild = mChildren.Exists( in_matchingKey.field );
			if (pChild)
			{
				in_currentKey.field = in_matchingKey.field;
				if ( pChild->template _ForEachMatchingEx<T_FCN,T_TOP_LVL_KEY>( in_fcn, in_matchingKey.childKey, in_curKeyTopLvl, in_currentKey.childKey, in_cookie ) )
				{
					pChild->Term();
					mChildren.Unset( in_matchingKey.field );
				}
			}
		}

		return IsEmpty();
	}

	bool _CheckException(const T_KEY& in_keyBeingSet /* ie 'value-key' */, const T_KEY& in_keyToCheck /* ie target-key */) const
	{
		bool bIsException = false;
		if (in_keyToCheck.IsFieldValid())
		{
			ChildTreeType* pChildTree = mChildren.Exists(in_keyToCheck.field);
			if (pChildTree == NULL)
				pChildTree = mChildren.Exists(T_KEY::GetInvalidFieldType::Get());//try looking for 'invalid key' ie. "unspecified" value.

			//When "in_keyBeingSet" has no more valid fields, and "in_keyToCheck" has a valid one, if there is a child tree than there is an exception value.
			if (!in_keyBeingSet.AnyFieldValid())
				bIsException = (pChildTree != NULL);

			else if (pChildTree != NULL)
				bIsException = pChildTree->_CheckException(in_keyBeingSet.childKey, in_keyToCheck.childKey);
		}
		return bIsException;
	}

protected:

	ChildArrayType mChildren;
};

//
// AkRTPCRootSearchTree
//

template <typename T_KEY, typename T_VALUE> 
class AkRTPCRootSearchTree: public AkRTPCSearchTreeCommon<T_KEY,T_VALUE>
{
public:
	typedef typename T_KEY::FieldType FieldType;
	struct RootValueType : public T_VALUE { FieldType key; };
	typedef AkSortedKeyArray<typename T_KEY::FieldType, RootValueType, ArrayPoolDefault > ItemArrayType;
	typedef AkRTPCSearchTreeCommon<T_KEY,T_VALUE> BaseClass;

	AkRTPCRootSearchTree(){}
	virtual ~AkRTPCRootSearchTree() { AKASSERT(mItems.IsEmpty()); }

	void Transfer( AkRTPCRootSearchTree<T_KEY,T_VALUE>& in_rhs )
	{
		BaseClass::Value() = (in_rhs.BaseClass::Value());
		BaseClass::m_bSet = (in_rhs.BaseClass::m_bSet);
		mItems.Transfer(in_rhs.mItems);
	}

	void Term()
	{
		mItems.Term();
	}

	T_VALUE* FindBestMatch( T_KEY& in_key )
	{
		T_VALUE* pVal = NULL;
		
		if (in_key.AnyFieldValid()) // (Only one field remains..)
			pVal = mItems.Exists(in_key);
		
		
		if (pVal)
		{
			return pVal;
		}
		else
		{
			in_key = T_KEY(); //clear field to return matching key.
			return AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		}
	}

	T_VALUE* FindExact( const T_KEY& in_key, T_VALUE** out_ppParentValue, bool* out_childValuesExist )
	{
		if (in_key.AnyFieldValid())
		{
			if ((out_ppParentValue != NULL) && (BaseClass::m_bSet))
				*out_ppParentValue = &(BaseClass::Value());

			if (out_childValuesExist != NULL)
				*out_childValuesExist = false;
			
			return mItems.Exists(in_key.field);
		}
		else
		{
			if (out_childValuesExist != NULL)
				*out_childValuesExist = !mItems.IsEmpty();
			
			return AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		}
	}

	void Unset( const T_KEY& in_key )
	{
		if (in_key.AnyFieldValid())
		{
			mItems.Unset(in_key.field);
			if ( mItems.IsEmpty() )
				mItems.Term();
		}
		else
		{
			return AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::UnsetValue();
		}
	}

	T_VALUE * Set( const T_KEY& in_key )
	{
		if (in_key.AnyFieldValid())
			return mItems.Set(in_key.field);
		else
			return AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::SetValue();
	}

	bool IsEmpty() const 
	{
		return !AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::m_bSet && mItems.IsEmpty();
	}

	void RemoveAll( const T_KEY& in_matchingKey )
	{
		if ( !in_matchingKey.IsFieldValid() )
		{
			AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::UnsetValue();
			Term();
		}
		else
		{
			mItems.Unset( in_matchingKey.field );
		}
	}

	template< typename T_FCN, typename T_TOP_LVL_KEY>
	void _ForEach( T_FCN in_fcn, T_TOP_LVL_KEY& in_curKeyTopLvl, T_KEY& in_currentKey, void* in_cookie )
	{
		T_VALUE* pValue = AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		if (pValue)
			in_fcn( *pValue, in_curKeyTopLvl, in_cookie );

		for( typename ItemArrayType::Iterator it = mItems.Begin(); it != mItems.End(); ++it)
		{
			in_currentKey.field = (*it).key;
			in_fcn( (*it), in_curKeyTopLvl, in_cookie );
		}
	}

	template< typename T_FCN, typename T_TOP_LVL_KEY>
	bool _ForEachEx( T_FCN in_fcn, T_TOP_LVL_KEY& in_curKeyTopLvl, T_KEY& in_currentKey, void* in_cookie )
	{
		T_VALUE* pValue = AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		if (pValue)
		{
			if ( in_fcn( *pValue, in_curKeyTopLvl, in_cookie ) )
				AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::UnsetValue();
		}

		for( typename ItemArrayType::Iterator it = mItems.Begin(); it != mItems.End(); )
		{
			in_currentKey.field = (*it).key;
			if ( in_fcn( (*it), in_curKeyTopLvl, in_cookie ) )
			{
				it = mItems.Erase(it);
			}
			else
			{
				++it;
			}
		}

		if (mItems.IsEmpty())
			mItems.Term();
	

		return IsEmpty();
	}

	template< typename T_FCN, typename T_TOP_LVL_KEY>
	inline void _ForEachMatching( T_FCN in_fcn, const T_KEY& in_matchingKey, T_TOP_LVL_KEY& in_curKeyTopLvl, T_KEY& in_currentKey, void* in_cookie )
	{
		T_VALUE* pValue = AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		if (pValue)
			in_fcn( *pValue, in_curKeyTopLvl, in_cookie );

		if ( !in_matchingKey.IsFieldValid() )
		{
			for( typename ItemArrayType::Iterator it = mItems.Begin(); it != mItems.End(); ++it)
			{
				in_currentKey.field = (*it).key;
				in_fcn( (*it), in_curKeyTopLvl, in_cookie );
			}
		}
		else
		{
			pValue = mItems.Exists( in_matchingKey.field );
			if (pValue)
			{
				in_currentKey.field = in_matchingKey.field;
				in_fcn( *pValue, in_curKeyTopLvl, in_cookie );
			}
		}
	}

	template< typename T_FCN, typename T_TOP_LVL_KEY>
	bool _ForEachMatchingEx( T_FCN in_fcn, const T_KEY& in_matchingKey, T_TOP_LVL_KEY& in_curKeyTopLvl, T_KEY& in_currentKey, void* in_cookie )
	{
		T_VALUE* pValue = AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::GetValue();
		if (pValue)
		{
			if ( in_fcn( *pValue, in_curKeyTopLvl, in_cookie ) )
				AkRTPCSearchTreeCommon<T_KEY,T_VALUE>::UnsetValue();
		}

		if ( !in_matchingKey.IsFieldValid() )
		{
			for( typename ItemArrayType::Iterator it = mItems.Begin(); it != mItems.End(); )
			{
				in_currentKey.field = (*it).key;
				if ( in_fcn( (*it), in_curKeyTopLvl, in_cookie ) )
				{
					it = mItems.Erase(it);
				}
				else
				{
					++it;
				}
			}

			if (mItems.IsEmpty())
				mItems.Term();
		}
		else
		{
			pValue = mItems.Exists( in_matchingKey.field );
			if (pValue)
			{
				in_currentKey.field = in_matchingKey.field;
				if ( in_fcn( *pValue, in_curKeyTopLvl, in_cookie ) )
					mItems.Unset( in_matchingKey.field );
			}
		}

		return IsEmpty();
	}

	bool _CheckException( const T_KEY& in_keyBeingSet, const T_KEY& in_keyToCheck ) const
	{
		if ( !in_keyBeingSet.IsFieldValid() && in_keyToCheck.IsFieldValid() )
			return ( mItems.Exists( in_keyToCheck.field ) != NULL );

		return false;
	}

private:
	ItemArrayType mItems;
};


// ---- Key Types ----

//Policy classes to check if a field is valid.
struct GetNullGameObjPtr	{ static inline CAkRegisteredObj* Get()		{return NULL;}						};
struct GetInvalidPlayingID	{ static inline AkPlayingID Get()			{return AK_INVALID_PLAYING_ID;}		};
struct GetInvalidUniqueID	{ static inline AkUniqueID Get()			{return AK_INVALID_UNIQUE_ID;}		};
struct GetInvalidMidiNote	{ static inline AkMidiNoteNo Get()			{return AK_INVALID_MIDI_NOTE;}		};
struct GetInvalidMidiCh		{ static inline AkMidiChannelNo Get()		{return AK_INVALID_MIDI_CHANNEL;}	};
struct GetNullPbiPtr		{ static inline CAkPBI* Get()				{return NULL;}						};

//
// AkKeyCommon
//

template <typename T_FIELD, typename T_GET_INVALID> 
class AkKeyCommon
{
public:
	typedef T_GET_INVALID GetInvalidFieldType;
	typedef T_FIELD FieldType;

	AkKeyCommon(): field( GetInvalidFieldType::Get() ) {} //<-- Field is initialized to the invalid value.
	AkKeyCommon(T_FIELD in_key): field(in_key) {}

	inline bool IsFieldValid() const { return field != GetInvalidFieldType::Get(); }

	T_FIELD field;

protected:
	inline T_FIELD CompField() const { return field; }
};

//
// Specialize comparison field function for MIDI channel and note;
// Invalid values MUST be less than all valid values!
//
template <> inline AkMidiChannelNo AkKeyCommon<AkMidiChannelNo,GetInvalidMidiCh>::CompField() const { return (field + 1) % 32; }
template <> inline AkMidiNoteNo AkKeyCommon<AkMidiNoteNo,GetInvalidMidiNote>::CompField() const { return (field + 1) % 256; }

//
// AkNestedKey
//

template <typename T_FIELD, typename T_GET_INVALID, typename T_CHILD_KEY_CLASS> 
class AkNestedKey: public AkKeyCommon<T_FIELD,T_GET_INVALID>
{
public:
	typedef AkNestedKey<T_FIELD,T_GET_INVALID,T_CHILD_KEY_CLASS> ThisKeyType;
	typedef AkKeyCommon<T_FIELD,T_GET_INVALID> CommonKeyType;
	typedef T_CHILD_KEY_CLASS ChildKeyType;
	typedef T_FIELD FieldType;

	enum { Index = 1 + ChildKeyType::Index };

	AkNestedKey(): AkKeyCommon<T_FIELD,T_GET_INVALID>() {}
	AkNestedKey(const T_FIELD& in_key, const T_CHILD_KEY_CLASS& in_childKey ): AkKeyCommon<T_FIELD,T_GET_INVALID>(in_key), childKey(in_childKey) {}

	//Convenience functions to satisfy picky compilers that require "AkKeyCommon<T_FIELD,T_GET_INVALID>::field" instead of just "field".
	inline T_FIELD& Field() { return CommonKeyType::field; }
	inline T_FIELD const& Field() const { return CommonKeyType::field; }

	inline bool operator<( const ThisKeyType& in_other ) const 
	{
		return ( this->CompField() < in_other.CompField() ) || ( Field() == in_other.Field() && childKey < in_other.childKey );
	}

	inline bool operator<=( const ThisKeyType& in_other ) const 
	{
		return ( this->CompField() < in_other.CompField() ) || ( Field() == in_other.Field() && childKey <= in_other.childKey );
	}

	inline bool operator>( const ThisKeyType& in_other ) const 
	{
		return ( this->CompField() > in_other.CompField() ) || ( Field() == in_other.Field() && childKey > in_other.childKey );
	}

	inline bool operator>=( const ThisKeyType& in_other ) const 
	{
		return ( this->CompField() > in_other.CompField() ) || ( Field() == in_other.Field() && childKey >= in_other.childKey );
	}

	inline bool operator ==( const ThisKeyType& in_other ) const 
	{
		return ( Field() == in_other.Field() ) && ( childKey == in_other.childKey );
	}

	inline bool operator !=( const ThisKeyType& in_other ) const 
	{
		return ( Field() != in_other.Field() ) || ( childKey != in_other.childKey );
	}

	inline bool AllFieldsValid() const 
	{ 
		return CommonKeyType::IsFieldValid() && childKey.AllFieldsValid(); 
	}

	inline bool AnyFieldValid() const 
	{ 
		return CommonKeyType::IsFieldValid() || childKey.AnyFieldValid(); 
	}

	// Same as operator == but invalid fields in 'in_matchingKey' will be treated as wildcards.
	inline bool MatchValidFields( const ThisKeyType& in_matchingKey ) const 
	{
		return ( !in_matchingKey.IsFieldValid() || this->Field() == in_matchingKey.field ) && 
				childKey.MatchValidFields( in_matchingKey.childKey );
	}

	//Returns true if this key has a narrower scope than in_other.  
	inline bool HasNarrowerScopeThan( const ThisKeyType& in_other ) const
	{
		return childKey.HasNarrowerScopeThan( in_other.childKey ) || ( this->IsFieldValid() && !in_other.IsFieldValid() );
	}

	//Returns true if a search algorithm would only have to visit a single subtree when this key is used as a matching key.  
	//	This means that all the keys are specified up to some specific sub-key, and then the rest of the nested keys are invalid.
	//  Alternatively this could also mean that no keys are specified (all invalid), and the single sub-tree is the entire tree.
	bool MatchesSingleSubtree() const
	{
		if ( CommonKeyType::IsFieldValid() )
			return childKey.MatchesSingleSubtree();
		else
			return !(childKey.AnyFieldValid());
	}

	AkUInt8 Bitfield() const
	{
		return ( this->IsFieldValid() ? 0x1 << Index : 0x0 ) | childKey.Bitfield();
	}

	bool Equal( const ThisKeyType& in_other, AkUInt8 in_Mask ) const
	{
		return ( ((in_Mask & (0x1 << Index)) != 0) ? this->Field() == in_other.Field() : true ) && childKey.Equal( in_other.childKey, in_Mask);
	}

	T_CHILD_KEY_CLASS childKey;
};

//
// AkRootKey
//

template <typename T_FIELD, typename T_GET_INVALID> 
class AkRootKey: public AkKeyCommon<T_FIELD,T_GET_INVALID>
{
public:
	typedef AkRootKey<T_FIELD,T_GET_INVALID> ThisKeyType;
	typedef AkKeyCommon<T_FIELD,T_GET_INVALID> CommonKeyType;

	enum { Index = 0 };

	AkRootKey(){}
	AkRootKey(const T_FIELD& in_item): AkKeyCommon<T_FIELD,T_GET_INVALID>(in_item) {}

	//Convenience functions to satisfy picky compilers that require "AkKeyCommon<T_FIELD,T_GET_INVALID>::field" instead of just "field".
	inline T_FIELD& Field() { return CommonKeyType::field; }
	inline T_FIELD const& Field() const { return CommonKeyType::field; }

	bool operator<( const ThisKeyType& in_other ) const { return this->CompField() < in_other.CompField(); }
	bool operator<=( const ThisKeyType& in_other ) const { return this->CompField() <= in_other.CompField(); }
	bool operator>( const ThisKeyType& in_other ) const { return this->CompField() > in_other.CompField(); }
	bool operator>=( const ThisKeyType& in_other ) const { return this->CompField() >= in_other.CompField(); }
	bool operator ==( const ThisKeyType& in_other ) const { return Field() == in_other.Field(); }
	bool operator !=( const ThisKeyType& in_other ) const { return Field() != in_other.Field(); }
	operator T_FIELD() const { return Field(); }

	inline bool AllFieldsValid() const { return CommonKeyType::IsFieldValid(); }
	inline bool AnyFieldValid() const { return CommonKeyType::IsFieldValid(); }

	inline bool MatchValidFields( const ThisKeyType& in_matchingKey ) const 
	{
		return ( !in_matchingKey.IsFieldValid() || this->Field() == in_matchingKey.Field() );
	}

	inline bool HasNarrowerScopeThan( const ThisKeyType& in_other ) const 
	{
		return ( CommonKeyType::IsFieldValid() && !in_other.IsFieldValid() );
	}

	bool MatchesSingleSubtree() const {return true;}

	AkUInt8 Bitfield() const
	{
		return ( this->IsFieldValid() ? 0x1 : 0x0 );
	}

	bool Equal( const ThisKeyType& in_other, AkUInt8 in_Mask ) const
	{
		return ( ((in_Mask & 0x1) != 0) ? this->Field() == in_other.Field() : true );
	}
};


class AkRTPCKey: public AkNestedKey< CAkRegisteredObj*, GetNullGameObjPtr,
							AkNestedKey< AkPlayingID, GetInvalidPlayingID,
								AkNestedKey< AkUniqueID, GetInvalidUniqueID,
									AkNestedKey< AkMidiChannelNo, GetInvalidMidiCh, 
										AkNestedKey< AkMidiNoteNo, GetInvalidMidiNote,
											AkRootKey< CAkPBI*, GetNullPbiPtr > > > > > >
{
public:
	AkRTPCKey() {}
	
	AkRTPCKey(CAkRegisteredObj* in_gameObj)
	{
		GameObj() = in_gameObj;
	}

	AkRTPCKey( CAkRegisteredObj* in_gameObj, AkPlayingID in_playingID )
	{
		GameObj() = in_gameObj;
		PlayingID() = in_playingID;
	}

	AkRTPCKey( CAkRegisteredObj* in_gameObj, AkPlayingID in_playingID, AkMidiNoteChannelPair midiNodeCh, AkUniqueID midiTarget )
	{
		GameObj() = in_gameObj;
		PlayingID() = in_playingID;
		MidiTargetID() = midiTarget;
		MidiChannelNo() = midiNodeCh.channel;
		MidiNoteNo() = midiNodeCh.note;
	}

	inline CAkRegisteredObj*& GameObj()						{ return field; }
	inline AkPlayingID& PlayingID() 						{ return childKey.field; }
	inline AkUniqueID& MidiTargetID()						{ return childKey.childKey.field; }
	inline AkMidiChannelNo& MidiChannelNo()					{ return childKey.childKey.childKey.field; }
	inline AkMidiNoteNo& MidiNoteNo()						{ return childKey.childKey.childKey.childKey.field; }
	inline CAkPBI*& PBI()									{ return childKey.childKey.childKey.childKey.childKey.field; }

	inline CAkRegisteredObj* const& GameObj() const			{ return field; }
	inline AkPlayingID const& PlayingID() const				{ return childKey.field; }
	inline AkUniqueID const& MidiTargetID() const			{ return childKey.childKey.field; }
	inline AkMidiChannelNo const& MidiChannelNo() const		{ return childKey.childKey.childKey.field; }
	inline AkMidiNoteNo const& MidiNoteNo() const			{ return childKey.childKey.childKey.childKey.field; }
	inline CAkPBI* const& PBI() const						{ return childKey.childKey.childKey.childKey.childKey.field; }
};

#define TAkRTPCKeyTree( T_VALUE ) \
			AkRTPCNestedSearchTree< AkRTPCKey, T_VALUE, \
				AkRTPCNestedSearchTree< AkRTPCKey::ChildKeyType, T_VALUE, \
					AkRTPCNestedSearchTree< AkRTPCKey::ChildKeyType::ChildKeyType, T_VALUE, \
						AkRTPCNestedSearchTree< AkRTPCKey::ChildKeyType::ChildKeyType::ChildKeyType, T_VALUE, \
							AkRTPCNestedSearchTree< AkRTPCKey::ChildKeyType::ChildKeyType::ChildKeyType::ChildKeyType, T_VALUE, \
								AkRTPCRootSearchTree< AkRTPCKey::ChildKeyType::ChildKeyType::ChildKeyType::ChildKeyType::ChildKeyType, T_VALUE> > > > > >

template <typename T_VALUE>
class AkRTPCKeyTree: public TAkRTPCKeyTree( T_VALUE )
{
public:
	AkRTPCKeyTree(): TAkRTPCKeyTree( T_VALUE )() {}

	//Return true if a more-specified (narrower-scoped) value for in_checkKey exists in the tree, when setting a value for in_valueSetKey.
	bool CheckException( const AkRTPCKey& in_valueSetKey, const AkRTPCKey& in_checkKey ) const
	{
		return in_checkKey.HasNarrowerScopeThan(in_valueSetKey) && TAkRTPCKeyTree( T_VALUE )::_CheckException( in_valueSetKey, in_checkKey );
	}
};

//
// RTPC tree structs
//

struct AkRTPCValue
{
	AkRTPCValue(): uValue(0) {}

	union{
		AkReal32 fValue;
		AkUInt32 uValue;
	};
};

typedef AkRTPCKeyTree< AkRTPCValue > AkRTPCValueTree;

//
// Modulator tree structs
//

class CAkModulatorCtx;

struct AkModTreeValue
{
	AkModTreeValue(): pCtx(0) {}

	CAkModulatorCtx* pCtx;
};

typedef AkRTPCKeyTree< AkModTreeValue > AkModulatorCtxTree;

#endif
