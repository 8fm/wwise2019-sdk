/***********************************************************************
The content of this file includes source code for the sound engine
portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
Two Source Code" as defined in the Source Code Addendum attached
with this file.  Any use of the Level Two Source Code shall be
subject to the terms and conditions outlined in the Source Code
Addendum and the End User License Agreement for Wwise(R).

Version:  Build: 
Copyright (c) 2006-2020 Audiokinetic Inc.
***********************************************************************/

#ifndef RTREE_H
#define RTREE_H

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSimdMath.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkDynaBlkPool.h>
#include <AK/Tools/Common/AkKeyArray.h>
#include <float.h>
#include <math.h>

//
// AkRTree.h
//

#define RTREE_TEMPLATE template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL, int TMAXNODES, int TMINNODES, typename TALLOC>
#define RTREE_QUAL AkRTree<DATATYPE, ELEMTYPE, NUMDIMS, ELEMTYPEREAL, TMAXNODES, TMINNODES, TALLOC>

#define RTREE_USE_SPHERICAL_VOLUME // Better split classification, may be slower on some systems
//#define AK_RTREE_PERF_MON

// Fwd decl
class RTFileStream;  // File I/O helper class, look below for implementation and notes.

/// \class AkRTree
/// Implementation of RTree, a multidimensional bounding rectangle tree.
/// Example usage: For a 3-dimensional tree use AkRTree<Object*, float, 3> myTree;
///
/// This modified, templated C++ version by Greg Douglas at Auran (http://www.auran.com)
///
/// DATATYPE Referenced data, should be int, void*, obj* etc. no larger than sizeof<void*> and simple type
/// ELEMTYPE Type of element such as int or float
/// NUMDIMS Number of dimensions such as 2 or 3
/// ELEMTYPEREAL Type of element that allows fractional and large values such as float or double, for use in volume calcs
///
/// NOTES: Inserting and removing data requires the knowledge of its constant Minimal Bounding Rectangle.

template<class DATATYPE, class ELEMTYPE, int NUMDIMS, class ELEMTYPEREAL = ELEMTYPE, int TMAXNODES = 8, int TMINNODES = TMAXNODES / 2, typename TALLOC = ArrayPoolDefault>
class AkRTree
{
protected:

	struct Node;  // Fwd decl.  Used by other internal structs and iterator
	
public:

	// These constant must be declared after Branch and before Node struct
	// Stuck up here for MSVC 6 compiler.  NSVC .NET 2003 is much happier.
	enum
	{
		MAXNODES = TMAXNODES,                         ///< Max elements in node
		MINNODES = TMINNODES,                         ///< Min elements in node
	};

	struct ResultsArray : public AkArray< DATATYPE, DATATYPE, TALLOC >
	{
		typedef AkArray< DATATYPE, DATATYPE, TALLOC > tArray;
		bool Add(DATATYPE& in_found) { return tArray::AddLast(in_found) != NULL; }
	};

public:

	AkRTree();
	virtual ~AkRTree();

	AKRESULT Init();
	void Term();

	/// Insert entry
	/// \param a_min Min of bounding rect
	/// \param a_max Max of bounding rect
	/// \param a_dataId Positive Id of data.  Maybe zero, but negative numbers not allowed.
	AKRESULT Insert(const AKSIMD_V4F32& a_min, const AKSIMD_V4F32& a_max, const DATATYPE& a_dataId);

	/// Remove entry
	/// \param a_min Min of bounding rect
	/// \param a_max Max of bounding rect
	/// \param a_dataId Positive Id of data.  Maybe zero, but negative numbers not allowed.
	AKRESULT Remove(const AKSIMD_V4F32& a_min, const AKSIMD_V4F32& a_max, const DATATYPE& a_dataId);

	/// Find all within search rectangle
	/// \param a_min Min of search bounding rect
	/// \param a_max Max of search bounding rect
	/// \param a_searchResult Search result array.  Caller should set grow size. Function will reset, not append to array.
	/// \return Returns the number of entries found
	template<typename SearchResults>
	int Search(const AKSIMD_V4F32& a_min, const AKSIMD_V4F32& a_max, SearchResults& a_searchResult, bool i_leafLevelOptimisation = false) const;
	
	/// Find all that intersect with a ray
	/// \param a_point ray starting point
	/// \param a_direction ray direction
	/// \param a_searchResult Search result array.  Caller should set grow size. Function will reset, not append to array.
	/// \return Returns the number of entries found
	template<typename SearchResults>
	int RaySearch(const AKSIMD_V4F32& a_point, const AKSIMD_V4F32& a_direction, SearchResults& a_searchResult, bool i_leafLevelOptimisation = true) const;

	/// Find the nearest object that intersect with a ray
	/// \param a_point ray starting point
	/// \param a_direction ray direction
	/// \param a_searchResult Search result array.  Caller should set grow size. Function will reset, not append to array.
	/// \return Returns the number of entries found
	template<typename SearchResults>
	int RayNearestSearch(const AKSIMD_V4F32& a_point, const AKSIMD_V4F32& a_direction, SearchResults& a_searchResult, bool i_leafLevelOptimisation = true) const;

	template<typename RayFrustum, typename RayBundle, typename SearchResults>
	int RayBundleNearestSearch(const RayFrustum& a_frustum, const AKSIMD_V4F32 &a_origin, const RayBundle& a_rays, SearchResults& a_searchResult, bool i_leafLevelOptimisation = true) const;

	template<typename SearchResults>
	int HalfspaceSetSearch(const AKSIMD_V4F32* a_planes, AkUInt32 a_numPlance, SearchResults& a_searchResult, bool i_leafLevelOptimisation = true) const;

	/// Remove all entries from tree
	AKRESULT RemoveAll();

	/// Count the data elements in this container.  This is slow as no internal counter is maintained.
	int Count();

	/// Load tree contents from file
	bool Load(const char* a_fileName);
	/// Load tree contents from stream
	bool Load(RTFileStream& a_stream);


	/// Save tree contents to file
	bool Save(const char* a_fileName);
	/// Save tree contents to stream
	bool Save(RTFileStream& a_stream);

	bool IsEmpty()
	{
		return m_root->m_count == 0;
	}

	/// Iterator is not remove safe.
	class Iterator
	{
	private:

		enum { MAX_STACK = 32 }; //  Max stack size. Allows almost n^32 where n is number of branches in node

		struct StackElement
		{
			Node* m_node;
			int m_branchIndex;
		};

	public:

		Iterator() { Init(); }

		~Iterator() { }

		/// Is iterator invalid
		bool IsNull() { return (m_tos <= 0); }

		/// Is iterator pointing to valid data
		bool IsNotNull() { return (m_tos > 0); }

		/// Access the current data element. Caller must be sure iterator is not NULL first.
		DATATYPE& operator*()
		{
			AKASSERT(IsNotNull());
			StackElement& curTos = m_stack[m_tos - 1];
			return curTos.m_node->m_data[curTos.m_branchIndex];
		}

		/// Access the current data element. Caller must be sure iterator is not NULL first.
		const DATATYPE& operator*() const
		{
			AKASSERT(IsNotNull());
			StackElement& curTos = m_stack[m_tos - 1];
			return curTos.m_node->m_data[curTos.m_branchIndex];
		}

		/// Find the next data element
		bool operator++() { return FindNextData(); }

		/// Get the bounds for this node
		void GetBounds(ELEMTYPE a_min[NUMDIMS], ELEMTYPE a_max[NUMDIMS])
		{
			AKASSERT(IsNotNull());
			StackElement& curTos = m_stack[m_tos - 1];

			for (int index = 0; index < NUMDIMS; ++index)
			{
				a_min[index] = curTos.m_node->m_rect.m_min[index];
				a_max[index] = curTos.m_node->m_rect.m_max[index];
			}
		}

	private:

		/// Reset iterator
		void Init() { m_tos = 0; }

		/// Find the next data element in the tree (For internal use only)
		bool FindNextData()
		{
			for (;;)
			{
				if (m_tos <= 0)
				{
					return false;
				}
				StackElement curTos = Pop(); // Copy stack top cause it may change as we use it

				if (curTos.m_node->IsLeaf())
				{
					// Keep walking through data while we can
					if (curTos.m_branchIndex + 1 < curTos.m_node->m_count)
					{
						// There is more data, just point to the next one
						Push(curTos.m_node, curTos.m_branchIndex + 1);
						return true;
					}
					// No more data, so it will fall back to previous level
				}
				else
				{
					if (curTos.m_branchIndex + 1 < curTos.m_node->m_count)
					{
						// Push sibling on for future tree walk
						// This is the 'fall back' node when we finish with the current level
						Push(curTos.m_node, curTos.m_branchIndex + 1);
					}
					// Since cur node is not a leaf, push first of next level to get deeper into the tree
					Node* nextLevelnode = curTos.m_node->m_child[curTos.m_branchIndex];
					Push(nextLevelnode, 0);

					// If we pushed on a new leaf, exit as the data is ready at TOS
					if (nextLevelnode->IsLeaf())
					{
						return true;
					}
				}
			}
		}

		/// Push node and branch onto iteration stack (For internal use only)
		void Push(Node* a_node, int a_branchIndex)
		{
			m_stack[m_tos].m_node = a_node;
			m_stack[m_tos].m_branchIndex = a_branchIndex;
			++m_tos;
			AKASSERT(m_tos <= MAX_STACK);
		}

		/// Pop element off iteration stack (For internal use only)
		StackElement& Pop()
		{
			AKASSERT(m_tos > 0);
			--m_tos;
			return m_stack[m_tos];
		}

		StackElement m_stack[MAX_STACK];              ///< Stack as we are doing iteration instead of recursion
		int m_tos;                                    ///< Top Of Stack index

		friend class AkRTree; // Allow hiding of non-public functions while allowing manipulation by logical owner
	};

	/// Get 'first' for iteration
	void GetFirst(Iterator& a_it)
	{
		a_it.Init();
		Node* first = m_root;
		while (first)
		{
			if (first->IsInternalNode() && first->m_count > 1)
			{
				a_it.Push(first, 1); // Descend sibling branch later
			}
			else if (first->IsLeaf())
			{
				if (first->m_count)
				{
					a_it.Push(first, 0);
				}
				break;
			}
			first = first->m_child[0];
		}
	}

	/// Get Next for iteration
	void GetNext(Iterator& a_it) { ++a_it; }

	/// Is iterator NULL, or at end?
	bool IsNull(Iterator& a_it) { return a_it.IsNull(); }

	/// Get object at iterator position
	DATATYPE& GetAt(Iterator& a_it) { return *a_it; }

protected:
	AK_ALIGN_SIMD(
	struct Ray
	{
		AKSIMD_V4F32 m_point;
		AKSIMD_V4F32 m_dir;
		AKSIMD_V4F32 m_invDir;
	});

	/// Minimal bounding rectangle (n-dimensional)
	AK_ALIGN_SIMD(
	struct Rect
	{
		AKSIMD_V4F32 m_min;
		AKSIMD_V4F32 m_max;
	});

	struct HalfspaceSet
	{
		const AKSIMD_V4F32* m_norm_d; //Array of vectors <x,y,z>, and scalar 'd', the distance from the origin.
		AkUInt32 m_num;			//number of halfspaces.
	};

	/// May be data or may be another subtree
	/// The parents level determines this.
	/// If the parents level is 0, then this is data
	struct Branch
	{
		Rect m_rect;                                  ///< Bounds
		Node* m_child;                                ///< Child node
		DATATYPE m_data;                              ///< Data Id
	};

	/// Node for each branch level
	struct Node
	{
		bool IsInternalNode() { return (m_level > 0); } // Not a leaf, but a internal node
		bool IsLeaf() { return (m_level == 0); } // A leaf, contains data

		Rect m_rect[MAXNODES];
		union
		{
			Node* m_child[MAXNODES];
			DATATYPE m_data[MAXNODES];
		};

		int m_count;                                  ///< Count
		int m_level;                                  ///< Leaf is zero, others positive
		
		void GetBranch(int idx, Branch& out_branch)
		{
			out_branch.m_rect = m_rect[idx];
			if (IsInternalNode())
				out_branch.m_child = m_child[idx];
			else
				out_branch.m_data = m_data[idx];
		}

		void SetBranch(int idx, const Branch& in_branch)
		{
			m_rect[idx] = in_branch.m_rect;
			if (IsInternalNode())
				m_child[idx] = in_branch.m_child;
			else
				m_data[idx] = in_branch.m_data;
		}

		void CopyBranch(int to, int from)
		{
			m_rect[to] = m_rect[from];
			if (IsInternalNode())
				m_child[to] = m_child[from];
			else
				m_data[to] = m_data[from];
		}
	};

	/// A link list of nodes for reinsertion after a delete operation
	struct ListNode
	{
		ListNode* m_next;                             ///< Next in list
		Node* m_node;                                 ///< Node
	};

	/// Store distance associated to a node
	/*struct NodeDistance
	{
		Node* m_node;
		float m_distance;

		bool operator<(const NodeDistance& i_nd) const { return m_distance < i_nd.m_distance; }
	};*/
	struct NodeDistance
	{
		Node* m_node;
		AkReal32 key;		
	};

	struct NullAllocator
	{
		AkForceInline void * Alloc(size_t in_uSize) { AKASSERT(false); return NULL; }
		AkForceInline void * ReAlloc(void * in_pAddress, size_t in_uOldSize, size_t in_uNewSize) { AKASSERT(false); return NULL; }
		AkForceInline void Free(void * in_pAddress) {}
		AkForceInline void TransferMem(void *& io_pDest, NullAllocator in_srcAlloc, void * in_pSrc) { io_pDest = in_pSrc; }
	};

	class NodeDistanceMap
		: public AkSortedKeyArray<AkReal32, NodeDistance, NullAllocator, AkGetArrayKey<AkReal32, NodeDistance>, AkGrowByPolicy_Legacy_SpatialAudio<0>>
	{
		typedef AkSortedKeyArray<AkReal32, NodeDistance, NullAllocator, AkGetArrayKey<AkReal32, NodeDistance>, AkGrowByPolicy_Legacy_SpatialAudio<0>> tBase;
	public:
		~NodeDistanceMap() { tBase::Term(); }

		/// Directly assign memory to the array.
		AKRESULT Reserve(NodeDistance * in_pAddr, AkUInt32 in_ulReserved)
		{
			AKASSERT(tBase::m_pItems == 0 && tBase::m_uLength == 0);

			tBase::m_ulReserved = 0;

			if (in_pAddr)
			{
				tBase::m_pItems = (NodeDistance *)in_pAddr;
				if (tBase::m_pItems == 0)
					return AK_InsufficientMemory;

				tBase::m_ulReserved = in_ulReserved;
			}

			return AK_Success;
		}
	};

	/// Variables for finding a split partition
	struct PartitionVars
	{
		enum { NOT_TAKEN = -1 }; // indicates that position

		int m_partition[MAXNODES + 1];
		int m_total;
		int m_minFill;
		int m_count[2];
		Rect m_cover[2];
		ELEMTYPEREAL m_area[2];

		Branch m_branchBuf[MAXNODES + 1];
		int m_branchCount;
		Rect m_coverSplit;
		ELEMTYPEREAL m_coverSplitArea;
	};

	Node* AllocNode(AKRESULT& result);
	void FreeNode(Node* a_node);
	void InitNode(Node* a_node);
	void InitRect(Rect* a_rect);
	bool InsertRectRec(const Branch& a_branch, Node* a_node, Node** a_newNode, int a_level, AKRESULT& result);
	bool InsertRect(const Branch& a_branch, Node** a_root, int a_level, AKRESULT& result);
	Rect NodeCover(Node* a_node);
	bool AddBranch(const Branch* a_branch, Node* a_node, Node** a_newNode, AKRESULT& result);
	void DisconnectBranch(Node* a_node, int a_index);
	int PickBranch(const Rect* a_rect, Node* a_node);
	Rect CombineRect(const Rect* a_rectA, const Rect* a_rectB);
	bool SplitNode(Node* a_node, const Branch* a_branch, Node** a_newNode, AKRESULT& result);
	ELEMTYPEREAL RectSphericalVolume(Rect* a_rect);
	ELEMTYPEREAL RectVolume(Rect* a_rect);
	ELEMTYPEREAL CalcRectVolume(Rect* a_rect);
	void GetBranches(Node* a_node, const Branch* a_branch, PartitionVars* a_parVars);
	void ChoosePartition(PartitionVars* a_parVars, int a_minFill);
	void LoadNodes(Node* a_nodeA, Node* a_nodeB, PartitionVars* a_parVars, AKRESULT& result);
	void InitParVars(PartitionVars* a_parVars, int a_maxRects, int a_minFill);
	void PickSeeds(PartitionVars* a_parVars);
	void Classify(int a_index, int a_group, PartitionVars* a_parVars);
	bool RemoveRect(Rect* a_rect, const DATATYPE& a_id, Node** a_root, AKRESULT& result);
	bool RemoveRectRec(Rect* a_rect, const DATATYPE& a_id, Node* a_node, ListNode** a_listNode, AKRESULT& result);
	ListNode* AllocListNode(AKRESULT& result);
	void FreeListNode(ListNode* a_listNode);
	bool Overlap(Rect* a_rectA, Rect* a_rectB) const;
	bool Overlap(Ray* a_rayA, Rect* a_rectB) const;
	bool Overlap(HalfspaceSet* a_halfspaces, Rect* a_rectB) const;
	bool Overlap(Rect* a_rectA, Rect* a_rectB, float& io_t) const;
	bool Overlap(Ray* a_rayA, Rect* a_rectB, float& io_t) const;
	bool Overlap(HalfspaceSet* a_halfspaces, Rect* a_rectB, float& io_t) const;
	
	template<typename RayFrustum>
	bool FrustumCulling(const RayFrustum& a_frustum, Rect* a_rect) const;

	AKRESULT ReInsert(Node* a_node, ListNode** a_listNode);
	template<typename SearchResults, typename Shape>
	bool Search(Node* a_node, Shape* a_rect, int& a_foundCount, SearchResults& a_searchResult, bool i_leafLevelOptimization) const;
	template<typename SearchResults, typename Shape>
	bool NearestSearch(Node* a_node, Shape* a_rect, int& a_foundCount, SearchResults& a_searchResult, bool i_leafLevelOptimization, float& io_tMin) const;
	
	template<typename RayFrustum, typename SearchResults, typename Shape>
	void RangedBundleSearch(const RayFrustum& a_frustum, Node* a_node, int a_rayNumber, Shape *a_rects, int a_firstIndex, SearchResults& a_searchResult, bool i_leafLevelOptimization, float *io_tMin) const;

	void RemoveAllRec(Node* a_node);
	void Reset();
	void CountRec(Node* a_node, int& a_count);

	bool SaveRec(Node* a_node, RTFileStream& a_stream);
	bool LoadRec(Node* a_node, RTFileStream& a_stream, AKRESULT& result);

	Node* m_root;                                    ///< Root of tree
	ELEMTYPEREAL m_unitSphereVolume;                 ///< Unit sphere constant for required number of dimensions
	AkDynaBlkPool<  Node, 4 * 1024 / sizeof(Node), TALLOC> m_nodePool;
	AkDynaBlkPool<  ListNode, 64, TALLOC> m_listNodePool;
};


// Because there is not stream support, this is a quick and dirty file I/O helper.
// Users will likely replace its usage with a Stream implementation from their favorite API.
class RTFileStream
{
public:


	RTFileStream()
	{
	}

	~RTFileStream()
	{
		Close();
	}

	bool OpenRead(const char* a_fileName)
	{
		return false;
	}

	bool OpenWrite(const char* a_fileName)
	{
		return false;
	}

	void Close()
	{
	}

	template< typename TYPE >
	size_t Write(const TYPE& a_value)
	{
		return 0;
	}

	template< typename TYPE >
	size_t WriteArray(const TYPE* a_array, int a_count)
	{
		return 0;
	}

	template< typename TYPE >
	size_t Read(TYPE& a_value)
	{
		return 0;
	}

	template< typename TYPE >
	size_t ReadArray(TYPE* a_array, int a_count)
	{
		return 0;
	}
};


RTREE_TEMPLATE
RTREE_QUAL::AkRTree(): m_root(NULL)
{}

RTREE_TEMPLATE
AKRESULT RTREE_QUAL::Init()
{
	AKASSERT(MAXNODES > MINNODES);
	AKASSERT(MINNODES > 0);

	// Precomputed volumes of the unit spheres for the first few dimensions
	const float UNIT_SPHERE_VOLUMES[] = {
		0.000000f, 2.000000f, 3.141593f, // Dimension  0,1,2
		4.188790f, 4.934802f, 5.263789f, // Dimension  3,4,5
		5.167713f, 4.724766f, 4.058712f, // Dimension  6,7,8
		3.298509f, 2.550164f, 1.884104f, // Dimension  9,10,11
		1.335263f, 0.910629f, 0.599265f, // Dimension  12,13,14
		0.381443f, 0.235331f, 0.140981f, // Dimension  15,16,17
		0.082146f, 0.046622f, 0.025807f, // Dimension  18,19,20 
	};

	AKRESULT res = AK_Success;
	m_root = AllocNode(res);
	if (res == AK_Success)
	{
		m_root->m_level = 0;
		m_unitSphereVolume = (ELEMTYPEREAL)UNIT_SPHERE_VOLUMES[NUMDIMS];
	}
	return res;
}

RTREE_TEMPLATE
void RTREE_QUAL::Term()
{
	Reset(); // Free, or reset node memory
}

RTREE_TEMPLATE
RTREE_QUAL::~AkRTree()
{
	Term();
}

RTREE_TEMPLATE
AKRESULT RTREE_QUAL::Insert(const AKSIMD_V4F32& a_min, const AKSIMD_V4F32& a_max, const DATATYPE& a_dataId)
{
	if (!m_root)
	{
		return AK_Fail;
	}

	AKASSERT(AKSIMD_MASK_V4F32(AKSIMD_LTEQ_V4F32(a_min, a_max)) == 0xf);
	
	Branch branch;
	branch.m_data = a_dataId;
	branch.m_child = NULL;

// 	for (int axis = 0; axis < NUMDIMS; ++axis)
// 	{
// 		AKASSERT(a_min[axis] <= a_max[axis]);
// 		AKASSERT(a_min[axis] == a_min[axis] && a_min[axis] >= -FLT_MAX);
// 		AKASSERT(a_max[axis] == a_max[axis] && a_max[axis] <= FLT_MAX);
// 		branch.m_rect.m_min[axis] = a_min[axis];
// 		branch.m_rect.m_max[axis] = a_max[axis];
// 	}
	branch.m_rect.m_min = a_min;
	branch.m_rect.m_max = a_max;

	AKRESULT res = AK_Success;
	InsertRect(branch, &m_root, 0, res);
	return res;
}


RTREE_TEMPLATE
AKRESULT RTREE_QUAL::Remove(const AKSIMD_V4F32& a_min, const AKSIMD_V4F32& a_max, const DATATYPE& a_dataId)
{
	if (!m_root)
	{
		return AK_Fail;
	}
	
	AKASSERT(AKSIMD_MASK_V4F32(AKSIMD_LTEQ_V4F32(a_min, a_max)) == 0xf);
	
	Rect rect;

// 	for (int axis = 0; axis < NUMDIMS; ++axis)
// 	{
// 		AKASSERT(a_min[axis] <= a_max[axis]);
// 		AKASSERT(a_min[axis] == a_min[axis] && a_min[axis] >= -FLT_MAX);
// 		AKASSERT(a_max[axis] == a_max[axis] && a_max[axis] <= FLT_MAX);
// 		rect.m_min[axis] = a_min[axis];
// 		rect.m_max[axis] = a_max[axis];
// 	}

	rect.m_min = a_min;
	rect.m_max = a_max;

	AKRESULT res = AK_Success;
	if (RemoveRect(&rect, a_dataId, &m_root, res) && res == AK_Success)
		res = AK_IDNotFound;
	return res;
}


RTREE_TEMPLATE
template<typename SearchResults>
int RTREE_QUAL::Search(const AKSIMD_V4F32& a_min, const AKSIMD_V4F32& a_max, SearchResults& a_searchResult, bool i_leafLevelOptimisation) const
{
	if (!m_root)
	{
		return 0;
	}

	AKASSERT(AKSIMD_MASK_V4F32(AKSIMD_LTEQ_V4F32(a_min, a_max)) == 0xf);
	
	Rect rect;
	rect.m_min = a_min;
	rect.m_max = a_max;

	int foundCount = 0;
	Search(m_root, &rect, foundCount, a_searchResult, i_leafLevelOptimisation);

	return foundCount;
}

RTREE_TEMPLATE
template<typename SearchResults>
int RTREE_QUAL::RaySearch(const AKSIMD_V4F32& a_point, const AKSIMD_V4F32& a_direction, SearchResults& a_searchResult, bool i_leafLevelOptimisation) const
{
	if (!m_root)
	{
		return 0;
	}

	static const AKSIMD_V4F32 one = AKSIMD_SET_V4F32(1.f);

	Ray ray;

	ray.m_point = a_point;
	ray.m_dir = a_direction;
	ray.m_invDir = AKSIMD_DIV_V4F32(one, ray.m_dir);

	int foundCount = 0;
	Search(m_root, &ray, foundCount, a_searchResult, i_leafLevelOptimisation);

	return foundCount;
}

RTREE_TEMPLATE
template<typename SearchResults>
int RTREE_QUAL::RayNearestSearch(const AKSIMD_V4F32& a_point, const AKSIMD_V4F32& a_direction, SearchResults& a_searchResult, bool i_leafLevelOptimisation) const
{
	AKASSERT(m_root != NULL); //<- Init() has not been called.
	static const AKSIMD_V4F32 one = AKSIMD_SET_V4F32(1.f);

	Ray ray;

	ray.m_point = a_point;
	ray.m_dir = a_direction;
	ray.m_invDir = AKSIMD_DIV_V4F32(one, ray.m_dir);

	int foundCount = 0;
	float t = FLT_MAX;
	NearestSearch(m_root, &ray, foundCount, a_searchResult, i_leafLevelOptimisation, t);

	return foundCount;
}

RTREE_TEMPLATE
template<typename RayFrustum, typename RayBundle, typename SearchResults>
int RTREE_QUAL::RayBundleNearestSearch(const RayFrustum& a_frustum, const AKSIMD_V4F32& a_origin, const RayBundle& a_rays, SearchResults& a_searchResult, bool i_leafLevelOptimisation) const
{
	AKASSERT(m_root != NULL); //<- Init() has not been called.
	static const AKSIMD_V4F32 one = AKSIMD_SET_V4F32(1.f);

	Ray rays[RayBundle::NumberOfRays::value];
	float t[RayBundle::NumberOfRays::value];
		
	RayFrustum frustum = a_frustum;
	for(int i=0; i<4; ++i)
	{
		frustum.m_invDirs[i] = AKSIMD_DIV_V4F32(one, frustum.m_dirs[i]);
	}

	for(unsigned int i=0; i<a_rays.GetNumberOfRays(); ++i)
	{
		rays[i].m_point = a_origin;
		rays[i].m_dir = a_rays.GetAt(i);
		rays[i].m_invDir = AKSIMD_DIV_V4F32(one, rays[i].m_dir);

		t[i] = FLT_MAX;	
	}

	RangedBundleSearch<RayFrustum, SearchResults, Ray>(frustum, m_root, a_rays.GetNumberOfRays(), rays, 0, a_searchResult, i_leafLevelOptimisation, t);

	return 0;
}


RTREE_TEMPLATE
template<typename SearchResults>
int RTREE_QUAL::HalfspaceSetSearch(const AKSIMD_V4F32* a_planes, AkUInt32 a_numPlanes, SearchResults& a_searchResult, bool i_leafLevelOptimisation) const
{
	if (!m_root)
	{
		return 0;
	}

	HalfspaceSet hss;

	hss.m_norm_d = a_planes;
	hss.m_num = a_numPlanes;

	int foundCount = 0;
	Search(m_root, &hss, foundCount, a_searchResult, i_leafLevelOptimisation);

	return foundCount;
}

RTREE_TEMPLATE
int RTREE_QUAL::Count()
{
	int count = 0;
	CountRec(m_root, count);

	return count;
}



RTREE_TEMPLATE
void RTREE_QUAL::CountRec(Node* a_node, int& a_count)
{
	if (a_node->IsInternalNode())  // not a leaf node
	{
		for (int index = 0; index < a_node->m_count; ++index)
		{
			CountRec(a_node->m_child[index], a_count);
		}
	}
	else // A leaf node
	{
		a_count += a_node->m_count;
	}
}


RTREE_TEMPLATE
bool RTREE_QUAL::Load(const char* a_fileName)
{
	RemoveAll(); // Clear existing tree

	RTFileStream stream;
	if (!stream.OpenRead(a_fileName))
	{
		return false;
	}

	bool result = Load(stream);

	stream.Close();

	return result;
}



RTREE_TEMPLATE
bool RTREE_QUAL::Load(RTFileStream& a_stream)
{
	// Write some kind of header
	int _dataFileId = ('R' << 0) | ('T' << 8) | ('R' << 16) | ('E' << 24);
	int _dataSize = sizeof(DATATYPE);
	int _dataNumDims = NUMDIMS;
	int _dataElemSize = sizeof(ELEMTYPE);
	int _dataElemRealSize = sizeof(ELEMTYPEREAL);
	int _dataMaxNodes = TMAXNODES;
	int _dataMinNodes = TMINNODES;

	int dataFileId = 0;
	int dataSize = 0;
	int dataNumDims = 0;
	int dataElemSize = 0;
	int dataElemRealSize = 0;
	int dataMaxNodes = 0;
	int dataMinNodes = 0;

	a_stream.Read(dataFileId);
	a_stream.Read(dataSize);
	a_stream.Read(dataNumDims);
	a_stream.Read(dataElemSize);
	a_stream.Read(dataElemRealSize);
	a_stream.Read(dataMaxNodes);
	a_stream.Read(dataMinNodes);

	bool result = false;

	// Test if header was valid and compatible
	if ((dataFileId == _dataFileId)
		&& (dataSize == _dataSize)
		&& (dataNumDims == _dataNumDims)
		&& (dataElemSize == _dataElemSize)
		&& (dataElemRealSize == _dataElemRealSize)
		&& (dataMaxNodes == _dataMaxNodes)
		&& (dataMinNodes == _dataMinNodes)
		)
	{
		// Recursively load tree
		result = LoadRec(m_root, a_stream);
	}

	return result;
}


RTREE_TEMPLATE
bool RTREE_QUAL::Save(const char* a_fileName)
{
	RTFileStream stream;
	if (!stream.OpenWrite(a_fileName))
	{
		return false;
	}

	bool result = Save(stream);

	stream.Close();

	return result;
}


RTREE_TEMPLATE
bool RTREE_QUAL::Save(RTFileStream& a_stream)
{
	// Write some kind of header
	int dataFileId = ('R' << 0) | ('T' << 8) | ('R' << 16) | ('E' << 24);
	int dataSize = sizeof(DATATYPE);
	int dataNumDims = NUMDIMS;
	int dataElemSize = sizeof(ELEMTYPE);
	int dataElemRealSize = sizeof(ELEMTYPEREAL);
	int dataMaxNodes = TMAXNODES;
	int dataMinNodes = TMINNODES;

	a_stream.Write(dataFileId);
	a_stream.Write(dataSize);
	a_stream.Write(dataNumDims);
	a_stream.Write(dataElemSize);
	a_stream.Write(dataElemRealSize);
	a_stream.Write(dataMaxNodes);
	a_stream.Write(dataMinNodes);

	// Recursively save tree
	bool result = SaveRec(m_root, a_stream);

	return result;
}


RTREE_TEMPLATE
AKRESULT RTREE_QUAL::RemoveAll()
{
	// Delete all existing nodes
	Reset();

	AKRESULT res = AK_Success;
	m_root = AllocNode(res);
	m_root->m_level = 0;
	
	return res;
}


RTREE_TEMPLATE
void RTREE_QUAL::Reset()
{
	if (m_root != NULL)
	{
		// Delete all existing nodes
		RemoveAllRec(m_root);
		m_root = NULL;
	}
}


RTREE_TEMPLATE
void RTREE_QUAL::RemoveAllRec(Node* a_node)
{
	AKASSERT(a_node);
	AKASSERT(a_node->m_level >= 0);

	if (a_node->IsInternalNode()) // This is an internal node in the tree
	{
		for (int index = 0; index < a_node->m_count; ++index)
		{
			RemoveAllRec(a_node->m_child[index]);
		}
	}
	FreeNode(a_node);
}


RTREE_TEMPLATE
typename RTREE_QUAL::Node* RTREE_QUAL::AllocNode(AKRESULT& out_result)
{
	Node* newNode = m_nodePool.New();
	
	if (newNode != NULL)
		InitNode(newNode);
	else
		out_result = AK_InsufficientMemory;

	return newNode;
}


RTREE_TEMPLATE
void RTREE_QUAL::FreeNode(Node* a_node)
{
	AKASSERT(a_node);
	m_nodePool.Delete(a_node);
}


// Allocate space for a node in the list used in DeletRect to
// store Nodes that are too empty.
RTREE_TEMPLATE
typename RTREE_QUAL::ListNode* RTREE_QUAL::AllocListNode(AKRESULT& result)
{
	typename RTREE_QUAL::ListNode* pListNode = m_listNodePool.New();
	
	if (pListNode == NULL)
		result = AK_InsufficientMemory;

	return pListNode;
}


RTREE_TEMPLATE
void RTREE_QUAL::FreeListNode(ListNode* a_listNode)
{
	m_listNodePool.Delete(a_listNode);
}


RTREE_TEMPLATE
void RTREE_QUAL::InitNode(Node* a_node)
{
	a_node->m_count = 0;
	a_node->m_level = -1;
}


RTREE_TEMPLATE
void RTREE_QUAL::InitRect(Rect* a_rect)
{
	a_rect->m_min = AKSIMD_SETZERO_V4F32();
	a_rect->m_max = AKSIMD_SETZERO_V4F32();
}


// Inserts a new data rectangle into the index structure.
// Recursively descends tree, propagates splits back up.
// Returns 0 if node was not split.  Old node updated.
// If node was split, returns 1 and sets the pointer pointed to by
// new_node to point to the new node.  Old node updated to become one of two.
// The level argument specifies the number of steps up from the leaf
// level to insert; e.g. a data rectangle goes in at level = 0.
RTREE_TEMPLATE
bool RTREE_QUAL::InsertRectRec(const Branch& a_branch, Node* a_node, Node** a_newNode, int a_level, AKRESULT& result)
{
	AKASSERT(a_node && a_newNode);
	AKASSERT(a_level >= 0 && a_level <= a_node->m_level);

	// recurse until we reach the correct level for the new record. data records
	// will always be called with a_level == 0 (leaf)
	if (a_node->m_level > a_level)
	{
		// Still above level for insertion, go down tree recursively
		Node* otherNode;

		// find the optimal branch for this record
		int index = PickBranch(&a_branch.m_rect, a_node);

		// recursively insert this record into the picked branch
		bool childWasSplit = InsertRectRec(a_branch, a_node->m_child[index], &otherNode, a_level, result);

		if (!childWasSplit)
		{
			// Child was not split. Merge the bounding box of the new record with the
			// existing bounding box
			a_node->m_rect[index] = CombineRect(&a_branch.m_rect, &(a_node->m_rect[index]));
			return false;
		}
		else
		{
			// Child was split. The old branches are now re-partitioned to two nodes
			// so we have to re-calculate the bounding boxes of each node
			a_node->m_rect[index] = NodeCover(a_node->m_child[index]);
			Branch branch;
			branch.m_child = otherNode;
			branch.m_rect = NodeCover(otherNode);

			// The old node is already a child of a_node. Now add the newly-created
			// node to a_node as well. a_node might be split because of that.
			return AddBranch(&branch, a_node, a_newNode, result);
		}
	}
	else if (a_node->m_level == a_level)
	{
		// We have reached level for insertion. Add rect, split if necessary
		return AddBranch(&a_branch, a_node, a_newNode, result);
	}
	else
	{
		// Should never occur
		AKASSERT(0);
		return false;
	}
}


// Insert a data rectangle into an index structure.
// InsertRect provides for splitting the root;
// returns 1 if root was split, 0 if it was not.
// The level argument specifies the number of steps up from the leaf
// level to insert; e.g. a data rectangle goes in at level = 0.
// InsertRect2 does the recursion.
//
RTREE_TEMPLATE
bool RTREE_QUAL::InsertRect(const Branch& a_branch, Node** a_root, int a_level, AKRESULT& result)
{
	AKASSERT(a_root);
	AKASSERT(a_level >= 0 && a_level <= (*a_root)->m_level);
#ifdef _DEBUG
	for (int index = 0; index < NUMDIMS; ++index)
	{
		AKASSERT(AKSIMD_GETELEMENT_V4F32(a_branch.m_rect.m_min,index) <= AKSIMD_GETELEMENT_V4F32(a_branch.m_rect.m_max,index));
	}
#endif //_DEBUG  

	Node* newNode;

	if (InsertRectRec(a_branch, *a_root, &newNode, a_level, result))  // Root split
	{
		// Grow tree taller and new root
		Node* newRoot = AllocNode(result);
		if (result == AK_Success)
		{
			newRoot->m_level = (*a_root)->m_level + 1;

			Branch branch;

			// add old root node as a child of the new root
			branch.m_rect = NodeCover(*a_root);
			branch.m_child = *a_root;
			AddBranch(&branch, newRoot, NULL, result);

			// add the split node as a child of the new root
			branch.m_rect = NodeCover(newNode);
			branch.m_child = newNode;
			AddBranch(&branch, newRoot, NULL, result);

			// set the new root as the root node
			*a_root = newRoot;

			return true;
		}
	}

	return false;
}


// Find the smallest rectangle that includes all rectangles in branches of a node.
RTREE_TEMPLATE
typename RTREE_QUAL::Rect RTREE_QUAL::NodeCover(Node* a_node)
{
	AKASSERT(a_node);

	Rect rect = a_node->m_rect[0];
	for (int index = 1; index < a_node->m_count; ++index)
	{
		rect = CombineRect(&rect, &(a_node->m_rect[index]));
	}

	return rect;
}


// Add a branch to a node.  Split the node if necessary.
// Returns 0 if node not split.  Old node updated.
// Returns 1 if node split, sets *new_node to address of new node.
// Old node updated, becomes one of two.
RTREE_TEMPLATE
bool RTREE_QUAL::AddBranch(const Branch* a_branch, Node* a_node, Node** a_newNode, AKRESULT& result)
{
	AKASSERT(a_branch);
	AKASSERT(a_node);

	if (a_node->m_count < MAXNODES)  // Split won't be necessary
	{
		a_node->SetBranch(a_node->m_count, *a_branch);
		++a_node->m_count;

		return false;
	}
	else
	{
		AKASSERT(a_newNode);

		return SplitNode(a_node, a_branch, a_newNode, result);;
	}
}


// Disconnect a dependent node.
// Caller must return (or stop using iteration index) after this as count has changed
RTREE_TEMPLATE
void RTREE_QUAL::DisconnectBranch(Node* a_node, int a_index)
{
	AKASSERT(a_node && (a_index >= 0) && (a_index < MAXNODES));
	AKASSERT(a_node->m_count > 0);

	// Remove element by swapping with the last element to prevent gaps in array
	//a_node->m_branch[a_index] = a_node->m_branch[a_node->m_count - 1];
	a_node->CopyBranch(a_index, a_node->m_count - 1);
	--a_node->m_count;
}


// Pick a branch.  Pick the one that will need the smallest increase
// in area to accomodate the new rectangle.  This will result in the
// least total area for the covering rectangles in the current node.
// In case of a tie, pick the one which was smaller before, to get
// the best resolution when searching.
RTREE_TEMPLATE
int RTREE_QUAL::PickBranch(const Rect* a_rect, Node* a_node)
{
	AKASSERT(a_rect && a_node);

	bool firstTime = true;
	ELEMTYPEREAL increase;
	ELEMTYPEREAL bestIncr = (ELEMTYPEREAL)-1;
	ELEMTYPEREAL area;
	ELEMTYPEREAL bestArea;
	int best;
	Rect tempRect;

	for (int index = 0; index < a_node->m_count; ++index)
	{
		Rect* curRect = &a_node->m_rect[index];
		area = CalcRectVolume(curRect);
		tempRect = CombineRect(a_rect, curRect);
		increase = CalcRectVolume(&tempRect) - area;
		if ((increase < bestIncr) || firstTime)
		{
			best = index;
			bestArea = area;
			bestIncr = increase;
			firstTime = false;
		}
		else if ((increase == bestIncr) && (area < bestArea))
		{
			best = index;
			bestArea = area;
			bestIncr = increase;
		}
	}
	return best;
}


// Combine two rectangles into larger one containing both
RTREE_TEMPLATE
typename RTREE_QUAL::Rect RTREE_QUAL::CombineRect(const Rect* a_rectA, const Rect* a_rectB)
{
	AKASSERT(a_rectA && a_rectB);

	Rect newRect;

	newRect.m_min = AKSIMD_MIN_V4F32(a_rectA->m_min, a_rectB->m_min);
	newRect.m_max = AKSIMD_MAX_V4F32(a_rectA->m_max, a_rectB->m_max);

	return newRect;
}



// Split a node.
// Divides the nodes branches and the extra one between two nodes.
// Old node is one of the new ones, and one really new one is created.
// Tries more than one method for choosing a partition, uses best result.
// Return true if node was successfully split.
RTREE_TEMPLATE
bool RTREE_QUAL::SplitNode(Node* a_node, const Branch* a_branch, Node** a_newNode, AKRESULT& result)
{
	AKASSERT(a_node);
	AKASSERT(a_branch);

	// Could just use local here, but member or external is faster since it is reused
	PartitionVars localVars;
	PartitionVars* parVars = &localVars;

	// Load all the branches into a buffer, initialize old node
	GetBranches(a_node, a_branch, parVars);

	// Find partition
	ChoosePartition(parVars, MINNODES);

	// Create a new node to hold (about) half of the branches
	*a_newNode = AllocNode(result);
	if (result == AK_Success)
	{
		(*a_newNode)->m_level = a_node->m_level;

		// Put branches from buffer into 2 nodes according to the chosen partition
		a_node->m_count = 0;
		LoadNodes(a_node, *a_newNode, parVars, result);

		AKASSERT((a_node->m_count + (*a_newNode)->m_count) == parVars->m_total);

		return true;
	}

	return false;
}


// Calculate the n-dimensional volume of a rectangle
RTREE_TEMPLATE
ELEMTYPEREAL RTREE_QUAL::RectVolume(Rect* a_rect)
{
	AKASSERT(a_rect);

	ELEMTYPEREAL volume = (ELEMTYPEREAL)1;

	for (int index = 0; index < NUMDIMS; ++index)
	{
		volume *= AKSIMD_GETELEMENT_V4F32(a_rect->m_max,index) - AKSIMD_GETELEMENT_V4F32(a_rect->m_min,index);
	}

	AKASSERT(volume >= (ELEMTYPEREAL)0);

	return volume;
}


// The exact volume of the bounding sphere for the given Rect
RTREE_TEMPLATE
ELEMTYPEREAL RTREE_QUAL::RectSphericalVolume(Rect* a_rect)
{
	AKASSERT(a_rect);

	ELEMTYPEREAL sumOfSquares = (ELEMTYPEREAL)0;
	ELEMTYPEREAL radius;

	for (int index = 0; index < NUMDIMS; ++index)
	{
		ELEMTYPEREAL halfExtent = ((ELEMTYPEREAL)AKSIMD_GETELEMENT_V4F32(a_rect->m_max,index) - (ELEMTYPEREAL)AKSIMD_GETELEMENT_V4F32(a_rect->m_min,index)) * 0.5f;
		sumOfSquares += halfExtent * halfExtent;
	}

	radius = (ELEMTYPEREAL)sqrt(sumOfSquares);

	// Pow maybe slow, so test for common dims like 2,3 and just use x*x, x*x*x.
	if (NUMDIMS == 3)
	{
		return (radius * radius * radius * m_unitSphereVolume);
	}
	else if (NUMDIMS == 2)
	{
		return (radius * radius * m_unitSphereVolume);
	}
	else
	{
		return (ELEMTYPEREAL)(pow(radius, NUMDIMS) * m_unitSphereVolume);
	}
}


// Use one of the methods to calculate retangle volume
RTREE_TEMPLATE
ELEMTYPEREAL RTREE_QUAL::CalcRectVolume(Rect* a_rect)
{
#ifdef RTREE_USE_SPHERICAL_VOLUME
	return RectSphericalVolume(a_rect); // Slower but helps certain merge cases
#else // RTREE_USE_SPHERICAL_VOLUME
	return RectVolume(a_rect); // Faster but can cause poor merges
#endif // RTREE_USE_SPHERICAL_VOLUME  
}


// Load branch buffer with branches from full node plus the extra branch.
RTREE_TEMPLATE
void RTREE_QUAL::GetBranches(Node* a_node, const Branch* a_branch, PartitionVars* a_parVars)
{
	AKASSERT(a_node);
	AKASSERT(a_branch);

	AKASSERT(a_node->m_count == MAXNODES);

	// Load the branch buffer
	for (int index = 0; index < MAXNODES; ++index)
	{
		a_node->GetBranch(index, a_parVars->m_branchBuf[index]);
	}
	a_parVars->m_branchBuf[MAXNODES] = *a_branch;
	a_parVars->m_branchCount = MAXNODES + 1;

	// Calculate rect containing all in the set
	a_parVars->m_coverSplit = a_parVars->m_branchBuf[0].m_rect;
	for (int index = 1; index < MAXNODES + 1; ++index)
	{
		a_parVars->m_coverSplit = CombineRect(&a_parVars->m_coverSplit, &a_parVars->m_branchBuf[index].m_rect);
	}
	a_parVars->m_coverSplitArea = CalcRectVolume(&a_parVars->m_coverSplit);
}


// Method #0 for choosing a partition:
// As the seeds for the two groups, pick the two rects that would waste the
// most area if covered by a single rectangle, i.e. evidently the worst pair
// to have in the same group.
// Of the remaining, one at a time is chosen to be put in one of the two groups.
// The one chosen is the one with the greatest difference in area expansion
// depending on which group - the rect most strongly attracted to one group
// and repelled from the other.
// If one group gets too full (more would force other group to violate min
// fill requirement) then other group gets the rest.
// These last are the ones that can go in either group most easily.
RTREE_TEMPLATE
void RTREE_QUAL::ChoosePartition(PartitionVars* a_parVars, int a_minFill)
{
	AKASSERT(a_parVars);

	ELEMTYPEREAL biggestDiff;
	int group, chosen, betterGroup;

	InitParVars(a_parVars, a_parVars->m_branchCount, a_minFill);
	PickSeeds(a_parVars);

	while (((a_parVars->m_count[0] + a_parVars->m_count[1]) < a_parVars->m_total)
		&& (a_parVars->m_count[0] < (a_parVars->m_total - a_parVars->m_minFill))
		&& (a_parVars->m_count[1] < (a_parVars->m_total - a_parVars->m_minFill)))
	{
		biggestDiff = (ELEMTYPEREAL)-1;
		for (int index = 0; index < a_parVars->m_total; ++index)
		{
			if (PartitionVars::NOT_TAKEN == a_parVars->m_partition[index])
			{
				Rect* curRect = &a_parVars->m_branchBuf[index].m_rect;
				Rect rect0 = CombineRect(curRect, &a_parVars->m_cover[0]);
				Rect rect1 = CombineRect(curRect, &a_parVars->m_cover[1]);
				ELEMTYPEREAL growth0 = CalcRectVolume(&rect0) - a_parVars->m_area[0];
				ELEMTYPEREAL growth1 = CalcRectVolume(&rect1) - a_parVars->m_area[1];
				ELEMTYPEREAL diff = growth1 - growth0;
				if (diff >= 0)
				{
					group = 0;
				}
				else
				{
					group = 1;
					diff = -diff;
				}

				AKASSERT(diff == diff);
				if (diff > biggestDiff)
				{
					biggestDiff = diff;
					chosen = index;
					betterGroup = group;
				}
				else if ((diff == biggestDiff) && (a_parVars->m_count[group] < a_parVars->m_count[betterGroup]))
				{
					chosen = index;
					betterGroup = group;
				}
			}
		}
		
		Classify(chosen, betterGroup, a_parVars);
	}

	// If one group too full, put remaining rects in the other
	if ((a_parVars->m_count[0] + a_parVars->m_count[1]) < a_parVars->m_total)
	{
		if (a_parVars->m_count[0] >= a_parVars->m_total - a_parVars->m_minFill)
		{
			group = 1;
		}
		else
		{
			group = 0;
		}
		for (int index = 0; index < a_parVars->m_total; ++index)
		{
			if (PartitionVars::NOT_TAKEN == a_parVars->m_partition[index])
			{
				Classify(index, group, a_parVars);
			}
		}
	}

	AKASSERT((a_parVars->m_count[0] + a_parVars->m_count[1]) == a_parVars->m_total);
	AKASSERT((a_parVars->m_count[0] >= a_parVars->m_minFill) &&
		(a_parVars->m_count[1] >= a_parVars->m_minFill));
}


// Copy branches from the buffer into two nodes according to the partition.
RTREE_TEMPLATE
void RTREE_QUAL::LoadNodes(Node* a_nodeA, Node* a_nodeB, PartitionVars* a_parVars, AKRESULT& result)
{
	AKASSERT(a_nodeA);
	AKASSERT(a_nodeB);
	AKASSERT(a_parVars);

	for (int index = 0; index < a_parVars->m_total; ++index)
	{
		AKASSERT(a_parVars->m_partition[index] == 0 || a_parVars->m_partition[index] == 1);

		int targetNodeIndex = a_parVars->m_partition[index];
		Node* targetNodes[] = { a_nodeA, a_nodeB };

		// It is assured that AddBranch here will not cause a node split. 
#ifndef AK_OPTIMIZED
		bool nodeWasSplit = AddBranch(&a_parVars->m_branchBuf[index], targetNodes[targetNodeIndex], NULL, result);
		AKASSERT(!nodeWasSplit);
#else
		AddBranch(&a_parVars->m_branchBuf[index], targetNodes[targetNodeIndex], NULL, result);
#endif
	}
}


// Initialize a PartitionVars structure.
RTREE_TEMPLATE
void RTREE_QUAL::InitParVars(PartitionVars* a_parVars, int a_maxRects, int a_minFill)
{
	AKASSERT(a_parVars);

	a_parVars->m_count[0] = a_parVars->m_count[1] = 0;
	a_parVars->m_area[0] = a_parVars->m_area[1] = (ELEMTYPEREAL)0;
	a_parVars->m_total = a_maxRects;
	a_parVars->m_minFill = a_minFill;
	for (int index = 0; index < a_maxRects; ++index)
	{
		a_parVars->m_partition[index] = PartitionVars::NOT_TAKEN;
	}
}


RTREE_TEMPLATE
void RTREE_QUAL::PickSeeds(PartitionVars* a_parVars)
{
	int seed0 = 0, seed1 = 1;
	ELEMTYPEREAL worst, waste;
	ELEMTYPEREAL area[MAXNODES + 1];

	for (int index = 0; index < a_parVars->m_total; ++index)
	{
		area[index] = CalcRectVolume(&a_parVars->m_branchBuf[index].m_rect);
	}

	worst = -FLT_MAX;
	for (int indexA = 0; indexA < a_parVars->m_total - 1; ++indexA)
	{
		for (int indexB = indexA + 1; indexB < a_parVars->m_total; ++indexB)
		{
			Rect oneRect = CombineRect(&a_parVars->m_branchBuf[indexA].m_rect, &a_parVars->m_branchBuf[indexB].m_rect);
			waste = CalcRectVolume(&oneRect) - area[indexA] - area[indexB];
			if (waste > worst)
			{
				worst = waste;
				seed0 = indexA;
				seed1 = indexB;
			}
		}
	}

	Classify(seed0, 0, a_parVars);
	Classify(seed1, 1, a_parVars);
}


// Put a branch in one of the groups.
RTREE_TEMPLATE
void RTREE_QUAL::Classify(int a_index, int a_group, PartitionVars* a_parVars)
{
	AKASSERT(a_parVars);
	AKASSERT(PartitionVars::NOT_TAKEN == a_parVars->m_partition[a_index]);

	a_parVars->m_partition[a_index] = a_group;

	// Calculate combined rect
	if (a_parVars->m_count[a_group] == 0)
	{
		a_parVars->m_cover[a_group] = a_parVars->m_branchBuf[a_index].m_rect;
	}
	else
	{
		a_parVars->m_cover[a_group] = CombineRect(&a_parVars->m_branchBuf[a_index].m_rect, &a_parVars->m_cover[a_group]);
	}

	// Calculate volume of combined rect
	a_parVars->m_area[a_group] = CalcRectVolume(&a_parVars->m_cover[a_group]);

	++a_parVars->m_count[a_group];
}


// Delete a data rectangle from an index structure.
// Pass in a pointer to a Rect, the tid of the record, ptr to ptr to root node.
// Returns 1 if record not found, 0 if success.
// RemoveRect provides for eliminating the root.
RTREE_TEMPLATE
bool RTREE_QUAL::RemoveRect(Rect* a_rect, const DATATYPE& a_id, Node** a_root, AKRESULT& result)
{
	AKASSERT(a_rect && a_root);
	AKASSERT(*a_root);

	ListNode* reInsertList = NULL;

	if (!RemoveRectRec(a_rect, a_id, *a_root, &reInsertList, result))
	{
		// Found and deleted a data item
		// Reinsert any branches from eliminated nodes
		while (reInsertList)
		{
			Node* tempNode = reInsertList->m_node;

			for (int index = 0; index < tempNode->m_count; ++index)
			{
				// TODO go over this code. should I use (tempNode->m_level - 1)?
				Branch br;
				tempNode->GetBranch(index, br);
				InsertRect(br,
					a_root,
					tempNode->m_level,
					result);
			}

			ListNode* remLNode = reInsertList;
			reInsertList = reInsertList->m_next;

			FreeNode(remLNode->m_node);
			FreeListNode(remLNode);
		}

		// Check for redundant root (not leaf, 1 child) and eliminate TODO replace
		// if with while? In case there is a whole branch of redundant roots...
		if ((*a_root)->m_count == 1 && (*a_root)->IsInternalNode())
		{
			Node* tempNode = (*a_root)->m_child[0];

			AKASSERT(tempNode);
			FreeNode(*a_root);
			*a_root = tempNode;
		}
		return false;
	}
	else
	{
		return true;
	}
}


// Delete a rectangle from non-root part of an index structure.
// Called by RemoveRect.  Descends tree recursively,
// merges branches on the way back up.
// Returns 1 if record not found, 0 if success.
RTREE_TEMPLATE
bool RTREE_QUAL::RemoveRectRec(Rect* a_rect, const DATATYPE& a_id, Node* a_node, ListNode** a_listNode, AKRESULT& result)
{
	AKASSERT(a_rect && a_node && a_listNode);
	AKASSERT(a_node->m_level >= 0);

	if (a_node->IsInternalNode())  // not a leaf node
	{
		for (int index = 0; index < a_node->m_count; ++index)
		{
			if (Overlap(a_rect, &(a_node->m_rect[index])))
			{
				if (!RemoveRectRec(a_rect, a_id, a_node->m_child[index], a_listNode, result))
				{
					if (a_node->m_child[index]->m_count >= MINNODES)
					{
						// child removed, just resize parent rect
						a_node->m_rect[index] = NodeCover(a_node->m_child[index]);
					}
					else
					{
						// child removed, not enough entries in node, eliminate node
						Node* childNode = a_node->m_child[index];
						AKRESULT res = ReInsert(childNode, a_listNode);
						DisconnectBranch(a_node, index); // Must return after this call as count has changed
						if (res != AK_Success)
						{
							FreeNode(childNode);
							result = res;
						}
					}
					return false;
				}
			}
		}
		return true;
	}
	else // A leaf node
	{
		for (int index = 0; index < a_node->m_count; ++index)
		{
			if (a_node->m_data[index] == a_id)
			{
				DisconnectBranch(a_node, index); // Must return after this call as count has changed
				return false;
			}
		}
		return true;
	}
}

// Decide whether two rectangles overlap.
RTREE_TEMPLATE
AkForceInline bool RTREE_QUAL::Overlap(Rect* a_rectA, Rect* a_rectB) const
{
	static float t;
	return Overlap(a_rectA, a_rectB, t);
}

RTREE_TEMPLATE
AkForceInline bool RTREE_QUAL::Overlap(Ray* a_rayA, Rect* a_rectB) const
{
	static float t;
	return Overlap(a_rayA, a_rectB, t);
}

RTREE_TEMPLATE
AkForceInline bool RTREE_QUAL::Overlap(HalfspaceSet* a_halfspaces, Rect* a_rectB) const
{	
	static float t;
	return Overlap(a_halfspaces, a_rectB, t);
}

// Decide whether two rectangles overlap.
RTREE_TEMPLATE
AkForceInline bool RTREE_QUAL::Overlap(Rect* a_rectA, Rect* a_rectB, float& io_t) const
{
	AKASSERT(a_rectA && a_rectB);

	AkUInt32 mask = AKSIMD_MASK_V4F32( AKSIMD_GT_V4F32(a_rectA->m_min, a_rectB->m_max) ) | 
					AKSIMD_MASK_V4F32( AKSIMD_GT_V4F32(a_rectB->m_min, a_rectA->m_max));

	// We don't know what to do for now
	io_t = FLT_MAX;

	return mask == 0;
}

RTREE_TEMPLATE
AkForceInline bool RTREE_QUAL::Overlap(Ray* a_rayA, Rect* a_rectB, float& io_t) const
{
	const AKSIMD_V4F32& p = a_rayA->m_point;
	const AKSIMD_V4F32& invD = a_rayA->m_invDir;

	const AKSIMD_V4F32& min = a_rectB->m_min;
	const AKSIMD_V4F32& max = a_rectB->m_max;

	static const AKSIMD_V4F32 one = AKSIMD_SET_V4F32(1.f);
	static const AKSIMD_V4F32 zero = AKSIMD_SET_V4F32(0.f);

	// Compute intersection t value of ray with near and far plane of slab 		
	AKSIMD_V4F32 t1 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(min, p), invD);
	AKSIMD_V4F32 t2 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(max, p), invD);

	AKSIMD_V4F32 tmin = AKSIMD_MAX_V4F32(AKSIMD_MIN_V4F32(t1, t2), zero);
	AKSIMD_V4F32 tmax = AKSIMD_MIN_V4F32(AKSIMD_MAX_V4F32(t2, t1), one);

	float maxComponent = AkMax(AKSIMD_GETELEMENT_V4F32(tmin, 0), AkMax(AKSIMD_GETELEMENT_V4F32(tmin, 1), AKSIMD_GETELEMENT_V4F32(tmin, 2)));
	float minComponent = AkMin(AKSIMD_GETELEMENT_V4F32(tmax, 0), AkMin(AKSIMD_GETELEMENT_V4F32(tmax, 1), AKSIMD_GETELEMENT_V4F32(tmax, 2)));

	io_t = minComponent;

	return maxComponent <= minComponent;
}

RTREE_TEMPLATE
AkForceInline bool RTREE_QUAL::Overlap(HalfspaceSet* a_halfspaces, Rect* a_rectB, float& io_t) const
{	
	// We don't know what to do for now
	io_t = FLT_MAX;

	// Convert AABB to center-extents representation
	static const AKSIMD_V4F32 vHalf = { 0.5f, 0.5f, 0.5f, 1.f };
	AKSIMD_V4F32 c = AKSIMD_MUL_V4F32(AKSIMD_ADD_V4F32(a_rectB->m_max, a_rectB->m_min), vHalf); // Compute AABB center
	AKSIMD_V4F32 e = AKSIMD_SUB_V4F32(a_rectB->m_max, c); // Compute positive extents

	for (AkUInt32 i=0; i<a_halfspaces->m_num; ++i)
	{
		// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
		//float r = e[0] * Abs(p.n[0]) + e[1] * Abs(p.n[1]) + e[2] * Abs(p.n[2]);
		AKSIMD_V4F32 r = AKSIMD_MUL_V4F32(e, AKSIMD_ABS_V4F32(a_halfspaces[i].m_norm_d));
		r = AKSIMD_HORIZONTALADD_V4F32(r);

		// Compute distance of box center from plane
		//float s = Dot(p.n, c) - p.d;
		AKSIMD_V4F32 s = AKSIMD_MUL_V4F32(a_halfspaces[i].m_norm_d, c);
		s = AKSIMD_HORIZONTALADD_V4F32(s);
		s = AKSIMD_ABS_V4F32(s);

		// Intersection occurs when distance s falls within [-r,+r] interval
		if (AKSIMD_MASK_V4F32(AKSIMD_LTEQ_V4F32(s, r)) == 0)
			return false;
	}

	return true;
}

RTREE_TEMPLATE
template<typename RayFrustum>
bool RTREE_QUAL::FrustumCulling(const RayFrustum& a_frustum, Rect* a_rect) const
{
	static const AKSIMD_V4F32 one = AKSIMD_SET_V4F32(1.f);
	static const AKSIMD_V4F32 zero = AKSIMD_SET_V4F32(0.f);

	const AKSIMD_V4F32& p = a_frustum.m_origin;
	const AKSIMD_V4F32& min = a_rect->m_min;
	const AKSIMD_V4F32& max = a_rect->m_max;

	AKSIMD_V4F32 minEntries = AKSIMD_SET_V4F32(FLT_MAX);
	AKSIMD_V4F32 maxExits = AKSIMD_SET_V4F32(FLT_MIN);

	AKSIMD_V4F32 minDirection = AKSIMD_SUB_V4F32(min, p);
	AKSIMD_V4F32 maxDirection = AKSIMD_SUB_V4F32(max, p);

	// At the end:
	// - minT contains minimum value for each axis.
	// - maxT contains maximum value for each axis.
	for (int i = 0; i < 4; ++i)
	{			
		const AKSIMD_V4F32& invD = a_frustum.m_invDirs[i];
		
		AKSIMD_V4F32 entries = AKSIMD_MUL_V4F32(minDirection, invD);
		AKSIMD_V4F32 exits = AKSIMD_MUL_V4F32(maxDirection, invD);

		minEntries = AKSIMD_MIN_V4F32(entries, minEntries);
		maxExits = AKSIMD_MAX_V4F32(exits, maxExits);
	}
	
	// Conditions for missing the aabb (1 or 2):
	// 1. In plane xy:
	//  - Minimum for x with mim slab > Maximum for y with max slab
	//  - Minimum for y with min slab > Maximum for x with max slab
	//
	// 2. In plane yz:
	//  - Minimum for y with min slab > Maximum for z with max slab
	//  - Minimum for z with min slab > Maximum for y with max slab

	return (AKSIMD_GETELEMENT_V4F32(minEntries, 0) > AKSIMD_GETELEMENT_V4F32(maxExits, 1) &&
		AKSIMD_GETELEMENT_V4F32(minEntries, 1) > AKSIMD_GETELEMENT_V4F32(maxExits, 0)
		) ||
		(AKSIMD_GETELEMENT_V4F32(minEntries, 1) > AKSIMD_GETELEMENT_V4F32(maxExits, 2) &&
			AKSIMD_GETELEMENT_V4F32(minEntries, 2) > AKSIMD_GETELEMENT_V4F32(maxExits, 1));
}

// Add a node to the reinsertion list.  All its branches will later
// be reinserted into the index structure.
RTREE_TEMPLATE
AKRESULT RTREE_QUAL::ReInsert(Node* a_node, ListNode** a_listNode)
{
	AKRESULT res = AK_Success;
	ListNode* newListNode;

	newListNode = AllocListNode(res);
	if (res == AK_Success)
	{
		newListNode->m_node = a_node;
		newListNode->m_next = *a_listNode;
		*a_listNode = newListNode;
	}
	return res;
}

// Search in an index tree or subtree for all data retangles that overlap the argument rectangle.
RTREE_TEMPLATE
template<typename SearchResults, typename Shape>
bool RTREE_QUAL::Search(Node* a_node, Shape* a_rect, int& a_foundCount, SearchResults& a_searchResult, bool i_leafLevelOptimization) const
{
	AKASSERT(a_node);
	AKASSERT(a_node->m_level >= 0);
	AKASSERT(a_rect);

#ifdef AK_RTREE_PERF_MON
	static AkUInt32 g_HitInternal = 0;
	static AkUInt32 g_MissInternal = 0;
	static AkUInt32 g_HitLeaf = 0;
	static AkUInt32 g_MissLeaf = 0;
#endif

	if (a_node->IsInternalNode())
	{
		// This is an internal node in the tree
		for (int index = 0; index < a_node->m_count; ++index)
		{
			if (Overlap(a_rect, &a_node->m_rect[index]))
			{
#ifdef AK_RTREE_PERF_MON
				g_HitInternal++;
#endif
				if (!Search(a_node->m_child[index], a_rect, a_foundCount, a_searchResult, i_leafLevelOptimization))
				{
					// The callback indicated to stop searching
					return false;
				}
			}
#ifdef AK_RTREE_PERF_MON
			else
			{
				g_MissInternal++;
			}
#endif
		}
	}
	else
	{
		// This is a leaf node
		for (int index = 0; index < a_node->m_count; ++index)
		{
			if (i_leafLevelOptimization || Overlap(a_rect, &a_node->m_rect[index]))
			{
#ifdef AK_RTREE_PERF_MON
				g_HitLeaf++;
#endif
				DATATYPE& id = a_node->m_data[index];
				++a_foundCount;

				if (!a_searchResult.Add(id))
					return false;
			}
#ifdef AK_RTREE_PERF_MON
			else
			{
				g_MissLeaf++;
			}
#endif
		}
	}

	return true; // Continue searching
}

// Search in an index tree or subtree for all data retangles that overlap the argument rectangle.
RTREE_TEMPLATE
template<typename SearchResults, typename Shape>
bool RTREE_QUAL::NearestSearch(Node* a_node, Shape* a_rect, int& a_foundCount, SearchResults& a_searchResult, bool i_leafLevelOptimization, float& io_tMin) const
{
	AKASSERT(a_node);
	AKASSERT(a_node->m_level >= 0);
	AKASSERT(a_rect);

	if (a_node->IsInternalNode())
	{
		// This is an internal node in the tree
		for (int index = 0; index < a_node->m_count; ++index)
		{			
			if (Overlap(a_rect, &a_node->m_rect[index]))
			{
				NearestSearch(a_node->m_child[index], a_rect, a_foundCount, a_searchResult, i_leafLevelOptimization, io_tMin);
			}			
		}
	}
	else
	{
		// This is a leaf node
		bool found = false;
		for (int index = 0; index < a_node->m_count; ++index)
		{			
			if (i_leafLevelOptimization || Overlap(a_rect, &a_node->m_rect[index]))
			{
				DATATYPE& id = a_node->m_data[index];
				++a_foundCount;

				if (!a_searchResult.Add(id))
				{
					found = true;
					io_tMin = AkMin(a_searchResult.GetMinDistance(), io_tMin);
				}

			}
		}

		if (!a_searchResult.FinalCheck())
		{
			found = true;
			io_tMin = AkMin(a_searchResult.GetMinDistance(), io_tMin);
		}

		return !found;
	}

	return true; // Continue searching
}

RTREE_TEMPLATE
template<typename RayFrustum, typename SearchResults, typename Shape>
void RTREE_QUAL::RangedBundleSearch(const RayFrustum& a_frustum, Node* a_node, int a_rayNumber, Shape *a_rects, int a_firstIndex, SearchResults& a_searchResult, bool i_leafLevelOptimization, float *io_tMin) const
{
	AKASSERT(a_node);
	AKASSERT(a_node->m_level >= 0);
	
	if (a_node->IsInternalNode())
	{
		// This is an internal node in the tree
		for (int index = 0; index < a_node->m_count; ++index)
		{
			/*if(!FrustumCulling(a_frustum, &a_node->m_rect[index]))*/
			{
				int r;
				for(r=a_firstIndex; r<a_rayNumber; ++r)
				{
					float t;
					if (Overlap(&a_rects[r], &a_node->m_rect[index], t))
					{
						io_tMin[r] = AkMin(t, io_tMin[r]);						
						break;
					}
				}
				
				if(r < a_rayNumber)
				{
					RangedBundleSearch<RayFrustum, SearchResults, Ray>(a_frustum, a_node->m_child[index], a_rayNumber, a_rects, r, a_searchResult, i_leafLevelOptimization, io_tMin);
				}
			}
		}
	}
	else
	{
		// This is a leaf node		
		for (int r = a_firstIndex; r < a_rayNumber; ++r)
		{
			// Parse all the triangles for this ray
			for (int index = 0; index < a_node->m_count; ++index)
			{				
				float t;
				if (i_leafLevelOptimization || Overlap(&a_rects[r], &a_node->m_rect[index], t))
				{
					io_tMin[r] = AkMin(t, io_tMin[r]);
					DATATYPE& id = a_node->m_data[index];
					if (!a_searchResult.Add(id, r))
					{
						io_tMin[r] = AkMin(a_searchResult.GetMinDistance(r), io_tMin[r]);
					}
				}
			}
				
			if (!a_searchResult.FinalCheck(r))
			{
				io_tMin[r] = AkMin(a_searchResult.GetMinDistance(r), io_tMin[r]);
			}
		}
	}
}

#undef RTREE_TEMPLATE
#undef RTREE_QUAL

#endif //RTREE_H

