// Copyright (c) 2008,2011  INRIA Sophia-Antipolis (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0+
//
//
// Author(s) : Camille Wormser, Pierre Alliez, Stephane Tayeb

#ifndef CGAL_AABB_TREE_H
#define CGAL_AABB_TREE_H

#include <CGAL/license/AABB_tree.h>

#include <CGAL/disable_warnings.h>

#include <vector>
#include <iterator>
#include <CGAL/internal/AABB_tree/AABB_traversal_traits.h>
#include <CGAL/internal/AABB_tree/AABB_node.h>
#include <CGAL/internal/AABB_tree/AABB_search_tree.h>
#include <CGAL/internal/AABB_tree/Has_nested_type_Shared_data.h>
#include <CGAL/internal/AABB_tree/Primitive_helper.h>
#include <boost/optional.hpp>
#include <boost/lambda/lambda.hpp>

#ifdef CGAL_HAS_THREADS
#include <CGAL/mutex.h>
#endif

/// \file AABB_tree.h

namespace CGAL {

/// \addtogroup PkgAABBTreeRef
/// @{

	/**
   * Class AABB_tree is a static data structure for efficient
   * intersection and distance computations in 3D. It builds a
   * hierarchy of axis-aligned bounding boxes (an AABB tree) from a set
   * of 3D geometric objects, and can receive intersection and distance
   * queries, provided that the corresponding predicates are
   * implemented in the traits class AABBTraits.
   * An instance of the class `AABBTraits` is internally stored.
   *
   * \sa `AABBTraits`
   * \sa `AABBPrimitive`
   *
   */
	template <typename AABBTraits>
	class AABB_tree
	{
	private:
		// internal KD-tree used to accelerate the distance queries
		typedef AABB_search_tree<AABBTraits> Search_tree;

		// type of the primitives container
		typedef std::vector<typename AABBTraits::Primitive> Primitives;

	public:
    typedef AABBTraits AABB_traits;
    
    /// \name Types
    ///@{

    /// Number type returned by the distance queries.
		typedef typename AABBTraits::FT FT;


    /// Type of 3D point.
		typedef typename AABBTraits::Point_3 Point;

    /// Type of input primitive.
		typedef typename AABBTraits::Primitive Primitive;
		/// Identifier for a primitive in the tree.
		typedef typename Primitive::Id Primitive_id;
		/// Unsigned integral size type.
		typedef typename Primitives::size_type size_type; 
    /// Type of bounding box.
		typedef typename AABBTraits::Bounding_box Bounding_box;
    /// 3D Point and Primitive Id type
		typedef typename AABBTraits::Point_and_primitive_id Point_and_primitive_id;
    /// \deprecated 
		typedef typename AABBTraits::Object_and_primitive_id Object_and_primitive_id;

    /*!
    An alias to `AABBTraits::Intersection_and_primitive_id<Query>`
    */
    #ifdef DOXYGEN_RUNNING
    template<typename Query>
    using Intersection_and_primitive_id = AABBTraits::Intersection_and_primitive_id<Query>;
    #else
    template<typename Query>
    struct Intersection_and_primitive_id {
      typedef typename AABBTraits::template Intersection_and_primitive_id<Query>::Type Type;
    };
    #endif

    
    ///@}

	public:
    /// \name Creation
    ///@{

    /// Constructs an empty tree, and initializes the internally stored traits
    /// class using `traits`.
    AABB_tree(const AABBTraits& traits = AABBTraits());

    /**
     * @brief Builds the datastructure from a sequence of primitives.
     * @param first iterator over first primitive to insert
     * @param beyond past-the-end iterator
     *
     * It is equivalent to constructing an empty tree and calling `insert(first,last,t...)`.
     * The tree stays empty if the memory allocation is not successful.
     */
		template<typename InputIterator,typename ... T>
		AABB_tree(InputIterator first, InputIterator beyond,T&& ...);  

    /// After one or more calls to `insert()` the internal data
    /// structure of the tree must be reconstructed. This procedure
    /// has a complexity of \f$O(n log(n))\f$, where \f$n\f$ is the number of
    /// primitives of the tree.  This procedure is called implicitly
    /// at the first call to a query member function. You can call
    /// `build()` explicitly to ensure that the next call to
    /// query functions will not trigger the reconstruction of the
    /// data structure.
    /// A call to `AABBTraits::set_shared_data(t...)`
    /// is made using the internally stored traits.
    template<typename ... T>
    void build(T&& ...);
#ifndef DOXYGEN_RUNNING
    void build();
#endif
    ///@}

		/// \name Operations
		///@{

    /// Equivalent to calling `clear()` and then `insert(first,last,t...)`.
		template<typename ConstPrimitiveIterator,typename ... T>
		void rebuild(ConstPrimitiveIterator first, ConstPrimitiveIterator beyond,T&& ...);


    /// Add a sequence of primitives to the set of primitives of the AABB tree.
    /// `%InputIterator` is any iterator and the parameter pack `T` are any types
    /// such that `Primitive` has a constructor with the following signature:
    /// `Primitive(%InputIterator, T...)`. If `Primitive` is a model of the concept
    /// `AABBPrimitiveWithSharedData`, a call to `AABBTraits::set_shared_data(t...)`
    /// is made using the internally stored traits.
		template<typename InputIterator,typename ... T>
		void insert(InputIterator first, InputIterator beyond,T&& ...);

    /// Adds a primitive to the set of primitives of the tree.
    inline void insert(const Primitive& p);

		/// Clears and destroys the tree.
		~AABB_tree()
		{
			clear();
		}
    /// Returns a const reference to the internally stored traits class.
    const AABBTraits& traits() const{
      return m_traits; 
    }
    
		/// Clears the tree.
		void clear()
		{
			// clear AABB tree
      clear_nodes();
			m_primitives.clear();
			clear_search_tree();
			m_default_search_tree_constructed = false;
		}

		/// Returns the axis-aligned bounding box of the whole tree.
		/// \pre `!empty()`
		const Bounding_box bbox() const { 
			CGAL_precondition(!empty());
			if(size() > 1)
				return root_node()->bbox(); 
			else
				return AABB_traits().compute_bbox_object()(m_primitives.begin(), 
																									 m_primitives.end());
		}
    
    /// Returns the number of primitives in the tree.
		size_type size() const { return m_primitives.size(); }
    
    /// Returns \c true, iff the tree contains no primitive.
		bool empty() const { return m_primitives.empty(); }
		///@}

	private:
    template <typename ... T>
    void set_primitive_data_impl(CGAL::Boolean_tag<false>,T ... ){}
    template <typename ... T>
    void set_primitive_data_impl(CGAL::Boolean_tag<true>,T&& ... t)
    {m_traits.set_shared_data(std::forward<T>(t)...);}

    template <typename ... T>
    void set_shared_data(T&& ...t){
      set_primitive_data_impl(CGAL::Boolean_tag<internal::Has_nested_type_Shared_data<Primitive>::value>(),std::forward<T>(t)...);
    }

		bool build_kd_tree() const;
		template<typename ConstPointIterator>
		bool build_kd_tree(ConstPointIterator first, ConstPointIterator beyond) const;
public:

    /// \name Intersection Tests
    ///@{

		/// Returns `true`, iff the query intersects at least one of
		/// the input primitives. \tparam Query must be a type for
		/// which `do_intersect` predicates are
		/// defined in the traits class `AABBTraits`.
		template<typename Query>
		bool do_intersect(const Query& query) const;

    /// Returns the number of primitives intersected by the
    /// query. \tparam Query must be a type for which
    /// `do_intersect` predicates are defined
    /// in the traits class `AABBTraits`.
		template<typename Query>
		size_type number_of_intersected_primitives(const Query& query) const;

    /// Outputs to the iterator the list of all intersected primitives
    /// ids. This function does not compute the intersection points
    /// and is hence faster than the function `all_intersections()`
    /// function below. \tparam Query must be a type for which
    /// `do_intersect` predicates are defined
    /// in the traits class `AABBTraits`.
		template<typename Query, typename OutputIterator>
		OutputIterator all_intersected_primitives(const Query& query, OutputIterator out) const;


    /// Returns the intersected primitive id that is encountered first 
		/// in the tree traversal, iff
    /// the query intersects at least one of the input primitives. No
    /// particular order is guaranteed over the tree traversal, such
    /// that, e.g, the primitive returned is not necessarily the
    /// closest from the source point of a ray query. \tparam Query
    /// must be a type for which
    /// `do_intersect` predicates are defined
    /// in the traits class `AABBTraits`.
		template <typename Query>
		boost::optional<Primitive_id> any_intersected_primitive(const Query& query) const;
    ///@}

    /// \name Intersections
    ///@{

    /// Outputs the list of all intersections, as objects of
    /// `Intersection_and_primitive_id<Query>::%Type`,
    /// between the query and the input data to
    /// the iterator. `do_intersect()`
    /// predicates and intersections must be defined for `Query`
    /// in the `AABBTraits` class.
		template<typename Query, typename OutputIterator>
		OutputIterator all_intersections(const Query& query, OutputIterator out) const;


    /// Returns the intersection that is encountered first 
		/// in the tree traversal. No particular
    /// order is guaranteed over the tree traversal, e.g, the
    /// primitive returned is not necessarily the closest from the
    /// source point of a ray query. Type `Query` must be a type
    /// for which `do_intersect` predicates
    /// and intersections are defined in the traits class AABBTraits.
		template <typename Query>
    boost::optional< typename Intersection_and_primitive_id<Query>::Type >
    any_intersection(const Query& query) const;



    /// Returns the intersection and  primitive id closest to the source point of the ray
    /// query.
    /// \tparam Ray must be the same as `AABBTraits::Ray_3` and
    /// `do_intersect` predicates and intersections for it must be
    /// defined.
    /// \tparam Skip a functor with an operator
    /// `bool operator()(const Primitive_id& id) const`
    /// that returns `true` in order to skip the primitive.
    /// Defaults to a functor that always returns `false`.
    ///
    /// \note `skip` might be given some primitives that are not intersected by `query`
    ///       because the intersection test is done after the skip test. Also note that
    ///       the order the primitives are given to `skip` is not necessarily the
    ///       intersection order with `query`.
    ///
    ///
    /// `AABBTraits` must be a model of `AABBRayIntersectionTraits` to
    /// call this member function.
    template<typename Ray, typename SkipFunctor>
    boost::optional< typename Intersection_and_primitive_id<Ray>::Type >
    first_intersection(const Ray& query, const SkipFunctor& skip) const;

    /// \cond
    template<typename Ray>
    boost::optional< typename Intersection_and_primitive_id<Ray>::Type >
    first_intersection(const Ray& query) const
    {
      return first_intersection(query, boost::lambda::constant(false));
    }
    /// \endcond

    /// Returns the primitive id closest to the source point of the ray
    /// query.
    /// \tparam Ray must be the same as `AABBTraits::Ray_3` and
    /// `do_intersect` predicates and intersections for it must be
    /// defined.
    /// \tparam Skip a functor with an operator
    /// `bool operator()(const Primitive_id& id) const`
    /// that returns `true` in order to skip the primitive.
    /// Defaults to a functor that always returns `false`.
    ///
    /// `AABBTraits` must be a model of `AABBRayIntersectionTraits` to
    /// call this member function.
    template<typename Ray, typename SkipFunctor>
    boost::optional<Primitive_id>
    first_intersected_primitive(const Ray& query, const SkipFunctor& skip) const;

    /// \cond
    template<typename Ray>
    boost::optional<Primitive_id>
    first_intersected_primitive(const Ray& query) const
    {
      return first_intersected_primitive(query, boost::lambda::constant(false));
    }
    /// \endcond
    ///@}

    /// \name Distance Queries
    ///@{

    /// Returns the minimum squared distance between the query point
    /// and all input primitives. Method
    /// `accelerate_distance_queries()` should be called before the
    /// first distance query, so that an internal secondary search
    /// structure is build, for improving performance.
		/// \pre `!empty()`
		FT squared_distance(const Point& query) const;

    /// Returns the point in the union of all input primitives which
    /// is closest to the query. In case there are several closest
    /// points, one arbitrarily chosen closest point is
    /// returned. Method `accelerate_distance_queries()` should be
    /// called before the first distance query, so that an internal
    /// secondary search structure is build, for improving
    /// performance.
		/// \pre `!empty()`
		Point closest_point(const Point& query) const;

    
    /// Returns a `Point_and_primitive_id` which realizes the
    /// smallest distance between the query point and all input
    /// primitives. Method `accelerate_distance_queries()` should be
    /// called before the first distance query, so that an internal
    /// secondary search structure is build, for improving
    /// performance.
		/// \pre `!empty()`
		Point_and_primitive_id closest_point_and_primitive(const Point& query) const;


    ///@}

    /// \name Accelerating the Distance Queries
    /// 
    /// In the following paragraphs, we discuss details of the
    /// implementation of the distance queries. We explain the
    /// internal use of hints, how the user can pass his own hints to
    /// the tree, and how the user can influence the construction of
    /// the secondary data structure used for accelerating distance
    /// queries.
    /// Internally, the distance queries algorithms are initialized
    /// with some hint, which has the same type as the return type of
    /// the query, and this value is refined along a traversal of the
    /// tree, until it is optimal, that is to say until it realizes
    /// the shortest distance to the primitives. In particular, the
    /// exact specification of these internal algorithms is that they
    /// minimize the distance to the object composed of the union of
    /// the primitives and the hint.
    /// It follows that 
    /// - in order to return the exact distance to the set of
    /// primitives, the algorithms need the hint to be exactly on the
    /// primitives;
    /// - if this is not the case, and if the hint happens to be closer
    /// to the query point than any of the primitives, then the hint
    /// is returned.
    ///
    /// This second observation is reasonable, in the sense that
    /// providing a hint to the algorithm means claiming that this
    /// hint belongs to the union of the primitives. These
    /// considerations about the hints being exactly on the primitives
    /// or not are important: in the case where the set of primitives
    /// is a triangle soup, and if some of the primitives are large,
    /// one may want to provide a much better hint than a vertex of
    /// the triangle soup could be. It could be, for example, the
    /// barycenter of one of the triangles. But, except with the use
    /// of an exact constructions kernel, one cannot easily construct
    /// points other than the vertices, that lie exactly on a triangle
    /// soup. Hence, providing a good hint sometimes means not being
    /// able to provide it exactly on the primitives. In rare
    /// occasions, this hint can be returned as the closest point.
    /// In order to accelerate distance queries significantly, the
    /// AABB tree builds an internal KD-tree containing a set of
    /// potential hints, when the method
    /// `accelerate_distance_queries()` is called. This KD-tree
    /// provides very good hints that allow the algorithms to run much
    /// faster than with a default hint (such as the
    /// `reference_point` of the first primitive). The set of
    /// potential hints is a sampling of the union of the primitives,
    /// which is obtained, by default, by calling the method
    /// `reference_point` of each of the primitives. However, such
    /// a sampling with one point per primitive may not be the most
    /// relevant one: if some primitives are very large, it helps
    /// inserting more than one sample on them. Conversely, a sparser
    /// sampling with less than one point per input primitive is
    /// relevant in some cases.
    ///@{

		/// Constructs internal search tree from
		/// a point set taken on the internal primitives
		/// returns `true` iff successful memory allocation
		bool accelerate_distance_queries() const;

    /// Constructs an internal KD-tree containing the specified point
    /// set, to be used as the set of potential hints for accelerating
    /// the distance queries.
    /// \tparam ConstPointIterator is an iterator with
    /// value type `Point_and_primitive_id`.
		template<typename ConstPointIterator>
		bool accelerate_distance_queries(ConstPointIterator first, ConstPointIterator beyond) const
		{
			#ifdef CGAL_HAS_THREADS
			//this ensures that this is done once at a time
			CGAL_SCOPED_LOCK(kd_tree_mutex);
			#endif
			clear_search_tree();
			m_default_search_tree_constructed = false; // not a default kd-tree
			return build_kd_tree(first,beyond);
		}
    
    /// Returns the minimum squared distance between the query point
    /// and all input primitives. The internal KD-tree is not used.
		/// \pre `!empty()`
		FT squared_distance(const Point& query, const Point& hint) const;

    /// Returns the point in the union of all input primitives which
    /// is closest to the query. In case there are several closest
    /// points, one arbitrarily chosen closest point is returned. The
    /// internal KD-tree is not used.
		/// \pre `!empty()`
		Point closest_point(const Point& query, const Point& hint) const;
    
    /// Returns a `Point_and_primitive_id` which realizes the
    /// smallest distance between the query point and all input
    /// primitives. The internal KD-tree is not used.
		/// \pre `!empty()`
		Point_and_primitive_id closest_point_and_primitive(const Point& query, const Point_and_primitive_id& hint) const;

    ///@}

	private:
    template<typename AABBTree, typename SkipFunctor>
    friend class AABB_ray_intersection;

    // clear nodes
    void clear_nodes()
    {
			if( size() > 1 ) {
				delete [] m_p_root_node;
			}
			m_p_root_node = nullptr;
    }

		// clears internal KD tree
		void clear_search_tree() const
		{
			if ( m_search_tree_constructed )
			{
				CGAL_assertion( m_p_search_tree!=nullptr );
				delete m_p_search_tree;
				m_p_search_tree = nullptr;
				m_search_tree_constructed = false;
                        }
		}

	public:

    /// \internal
		template <class Query, class Traversal_traits>
		void traversal(const Query& query, Traversal_traits& traits) const
		{
			switch(size())
			{
			case 0:
				break;
			case 1:
				traits.intersection(query, singleton_data());
				break;
			default: // if(size() >= 2)
				root_node()->template traversal<Traversal_traits,Query>(query, traits, m_primitives.size());
			}
		}

	private:
		typedef AABB_node<AABBTraits> Node;


	public:
		// returns a point which must be on one primitive
		Point_and_primitive_id any_reference_point_and_id() const
		{
			CGAL_assertion(!empty());
			return Point_and_primitive_id(
        internal::Primitive_helper<AABB_traits>::get_reference_point(m_primitives[0],m_traits), m_primitives[0].id()
      );
		}

	public:
		Point_and_primitive_id best_hint(const Point& query) const
		{
			if(m_search_tree_constructed)
                        {
				return m_p_search_tree->closest_point(query);
                        }
			else
				return this->any_reference_point_and_id();
		}
		
		//! Returns the datum (geometric object) represented `p`. 
#ifndef DOXYGEN_RUNNING
		typename internal::Primitive_helper<AABBTraits>::Datum_type 
#else
		typename AABBTraits::Primitive::Datum_reference 
#endif
		datum(Primitive& p)const
		{
		  return internal::Primitive_helper<AABBTraits>::
		      get_datum(p, this->traits());
		}

	private:
    //Traits class
    AABBTraits m_traits;
		// set of input primitives
		Primitives m_primitives;
		// single root node
		Node* m_p_root_node;
    #ifdef CGAL_HAS_THREADS
    mutable CGAL_MUTEX internal_tree_mutex;//mutex used to protect const calls inducing build()
    mutable CGAL_MUTEX kd_tree_mutex;//mutex used to protect calls to accelerate_distance_queries
    #endif
  
    const Node* root_node() const {
			CGAL_assertion(size() > 1);
      if(m_need_build){
        #ifdef CGAL_HAS_THREADS
        //this ensures that build() will be called once
        CGAL_SCOPED_LOCK(internal_tree_mutex);
        if(m_need_build)
        #endif
          const_cast< AABB_tree<AABBTraits>* >(this)->build(); 
      }
      return m_p_root_node;
    }

		const Primitive& singleton_data() const {
			CGAL_assertion(size() == 1);
			return *m_primitives.begin();
		}

		// search KD-tree
		mutable const Search_tree* m_p_search_tree;
		mutable bool m_search_tree_constructed;
    mutable bool m_default_search_tree_constructed; // indicates whether the internal kd-tree should be built
    bool m_need_build;

	private:
		// Disabled copy constructor & assignment operator
		typedef AABB_tree<AABBTraits> Self;
		AABB_tree(const Self& src);
		Self& operator=(const Self& src);

	};  // end class AABB_tree

/// @}

  template<typename Tr>
  AABB_tree<Tr>::AABB_tree(const Tr& traits)
    : m_traits(traits)
    , m_primitives()
    , m_p_root_node(nullptr)
    , m_p_search_tree(nullptr)
    , m_search_tree_constructed(false)
    , m_default_search_tree_constructed(false)
    , m_need_build(false)
  {}

 	template<typename Tr>
	template<typename ConstPrimitiveIterator, typename ... T>
	AABB_tree<Tr>::AABB_tree(ConstPrimitiveIterator first,
                           ConstPrimitiveIterator beyond,
                           T&& ... t)
		: m_traits()
    , m_primitives()
		, m_p_root_node(nullptr)
		, m_p_search_tree(nullptr)
		, m_search_tree_constructed(false)
    , m_default_search_tree_constructed(false)
    , m_need_build(false)
	{
		// Insert each primitive into tree
    insert(first, beyond,std::forward<T>(t)...);
 	}
  
	template<typename Tr>
	template<typename ConstPrimitiveIterator, typename ... T>
	void AABB_tree<Tr>::insert(ConstPrimitiveIterator first,
                             ConstPrimitiveIterator beyond,
                             T&& ... t)
	{
		set_shared_data(std::forward<T>(t)...);
		while(first != beyond)
		{
			m_primitives.push_back(Primitive(first,std::forward<T>(t)...));
			++first;
		}
    m_need_build = true;
  }
  
  // Clears tree and insert a set of primitives
	template<typename Tr>
	template<typename ConstPrimitiveIterator, typename ... T>
	void AABB_tree<Tr>::rebuild(ConstPrimitiveIterator first,
                              ConstPrimitiveIterator beyond,
                              T&& ... t)
	{
		// cleanup current tree and internal KD tree
		clear();

		// inserts primitives
    insert(first, beyond,std::forward<T>(t)...);

    build();
	}  
        
        template<typename Tr>
        template<typename ... T>
        void AABB_tree<Tr>::build(T&& ... t)
        {
          set_shared_data(std::forward<T>(t)...);
          build();
        }

	template<typename Tr>
	void AABB_tree<Tr>::insert(const Primitive& p)
	{
    m_primitives.push_back(p);
    m_need_build = true;
  }

	// Build the data structure, after calls to insert(..)
	template<typename Tr>
	void AABB_tree<Tr>::build()
	{
    clear_nodes();

    if(m_primitives.size() > 1) {

			// allocates tree nodes
			m_p_root_node = new Node[m_primitives.size()-1]();
			if(m_p_root_node == nullptr)
			{
				std::cerr << "Unable to allocate memory for AABB tree" << std::endl;
				CGAL_assertion(m_p_root_node != nullptr);
				m_primitives.clear();
				clear();
			}

			// constructs the tree
			m_p_root_node->expand(m_primitives.begin(), m_primitives.end(),
					      m_primitives.size(), m_traits);
		}


		// In case the users has switched on the accelerated distance query
		// data structure with the default arguments, then it has to be
		// /built/rebuilt.
		if(m_default_search_tree_constructed)
			build_kd_tree();
		m_need_build = false;
	}
	// constructs the search KD tree from given points
	// to accelerate the distance queries
	template<typename Tr>
	bool AABB_tree<Tr>::build_kd_tree() const
	{
		// iterate over primitives to get reference points on them
		std::vector<Point_and_primitive_id> points;
		points.reserve(m_primitives.size());
		typename Primitives::const_iterator it;
		for(it = m_primitives.begin(); it != m_primitives.end(); ++it)
			points.push_back( Point_and_primitive_id(
				internal::Primitive_helper<AABB_traits>::get_reference_point(
					*it,m_traits), it->id() ) );

		// clears current KD tree
		clear_search_tree();
		bool res = build_kd_tree(points.begin(), points.end());
		m_default_search_tree_constructed = true;
		return res;
	}

	// constructs the search KD tree from given points
	// to accelerate the distance queries
	template<typename Tr>
	template<typename ConstPointIterator>
	bool AABB_tree<Tr>::build_kd_tree(ConstPointIterator first,
		ConstPointIterator beyond) const
	{
		m_p_search_tree = new Search_tree(first, beyond);
                m_default_search_tree_constructed = true;
		if(m_p_search_tree != nullptr)
		{
			m_search_tree_constructed = true;
			return true;
		}
		else
    {
			std::cerr << "Unable to allocate memory for accelerating distance queries" << std::endl;
			return false;
    }
	}

	// constructs the search KD tree from internal primitives
	template<typename Tr>
	bool AABB_tree<Tr>::accelerate_distance_queries() const
	{
		if(m_primitives.empty()) return true;
		if (m_default_search_tree_constructed)
		{
			if (!m_need_build) return m_search_tree_constructed;
			return true; // default return type, no tree built
		}

		if(!m_need_build) // the tree was already built, build the kd-tree
		{
			#ifdef CGAL_HAS_THREADS
			//this ensures that this function will be done once
			CGAL_SCOPED_LOCK(kd_tree_mutex);
			#endif
			if (!m_need_build)
			{
				// clears current KD tree
				clear_search_tree();
				bool res = build_kd_tree();
				m_default_search_tree_constructed = true;
				return res;
			};
		}
		m_default_search_tree_constructed = true;
		return m_search_tree_constructed;
	}

	template<typename Tr>
	template<typename Query>
	bool
		AABB_tree<Tr>::do_intersect(const Query& query) const
	{
    using namespace CGAL::internal::AABB_tree;
    typedef typename AABB_tree<Tr>::AABB_traits AABBTraits;
		Do_intersect_traits<AABBTraits, Query> traversal_traits(m_traits);
		this->traversal(query, traversal_traits);
		return traversal_traits.is_intersection_found();
	}
#ifndef DOXYGEN_RUNNING //To avoid doxygen to consider definition and declaration as 2 different functions (size_type causes problems)
	template<typename Tr>
	template<typename Query>
	typename AABB_tree<Tr>::size_type
		AABB_tree<Tr>::number_of_intersected_primitives(const Query& query) const
	{
    using namespace CGAL::internal::AABB_tree;
    using CGAL::internal::AABB_tree::Counting_output_iterator;
    typedef typename AABB_tree<Tr>::AABB_traits AABBTraits;
    typedef Counting_output_iterator<Primitive_id, size_type> Counting_iterator;

    size_type counter = 0;
    Counting_iterator out(&counter);

		Listing_primitive_traits<AABBTraits, 
      Query, Counting_iterator> traversal_traits(out,m_traits);
		this->traversal(query, traversal_traits);
		return counter;
	}
#endif
	template<typename Tr>
	template<typename Query, typename OutputIterator>
	OutputIterator
		AABB_tree<Tr>::all_intersected_primitives(const Query& query,
		OutputIterator out) const
	{
    using namespace CGAL::internal::AABB_tree;
    typedef typename AABB_tree<Tr>::AABB_traits AABBTraits;
		Listing_primitive_traits<AABBTraits, 
      Query, OutputIterator> traversal_traits(out,m_traits);
		this->traversal(query, traversal_traits);
		return out;
	}

	template<typename Tr>
	template<typename Query, typename OutputIterator>
	OutputIterator
		AABB_tree<Tr>::all_intersections(const Query& query,
		OutputIterator out) const
	{
    using namespace CGAL::internal::AABB_tree;
    typedef typename AABB_tree<Tr>::AABB_traits AABBTraits;
		Listing_intersection_traits<AABBTraits, 
      Query, OutputIterator> traversal_traits(out,m_traits);
		this->traversal(query, traversal_traits);
		return out;
	}


	template <typename Tr>
	template <typename Query>
  boost::optional< typename AABB_tree<Tr>::template Intersection_and_primitive_id<Query>::Type >
		AABB_tree<Tr>::any_intersection(const Query& query) const
	{
    using namespace CGAL::internal::AABB_tree;
    typedef typename AABB_tree<Tr>::AABB_traits AABBTraits;
		First_intersection_traits<AABBTraits, Query> traversal_traits(m_traits);
		this->traversal(query, traversal_traits);
		return traversal_traits.result();
	}

	template <typename Tr>
	template <typename Query>
	boost::optional<typename AABB_tree<Tr>::Primitive_id>
		AABB_tree<Tr>::any_intersected_primitive(const Query& query) const
	{
    using namespace CGAL::internal::AABB_tree;
    typedef typename AABB_tree<Tr>::AABB_traits AABBTraits;
		First_primitive_traits<AABBTraits, Query> traversal_traits(m_traits);
		this->traversal(query, traversal_traits);
		return traversal_traits.result();
	}

	// closest point with user-specified hint
	template<typename Tr>
	typename AABB_tree<Tr>::Point
		AABB_tree<Tr>::closest_point(const Point& query,
		const Point& hint) const
	{
		CGAL_precondition(!empty());
		typename Primitive::Id hint_primitive = m_primitives[0].id();
    using namespace CGAL::internal::AABB_tree;
    typedef typename AABB_tree<Tr>::AABB_traits AABBTraits;
		Projection_traits<AABBTraits> projection_traits(hint,hint_primitive,m_traits);
		this->traversal(query, projection_traits);
		return projection_traits.closest_point();
	}

	// closest point without hint, the search KD-tree is queried for the
	// first closest neighbor point to get a hint
	template<typename Tr>
	typename AABB_tree<Tr>::Point
		AABB_tree<Tr>::closest_point(const Point& query) const
	{
		CGAL_precondition(!empty());
		const Point_and_primitive_id hint = best_hint(query);
		return closest_point(query,hint.first);
	}

	// squared distance with user-specified hint
	template<typename Tr>
	typename AABB_tree<Tr>::FT
		AABB_tree<Tr>::squared_distance(const Point& query,
		const Point& hint) const
	{
		CGAL_precondition(!empty());
		const Point closest = this->closest_point(query, hint);
		return Tr().squared_distance_object()(query, closest);
	}

	// squared distance without user-specified hint
	template<typename Tr>
	typename AABB_tree<Tr>::FT
		AABB_tree<Tr>::squared_distance(const Point& query) const
	{
		CGAL_precondition(!empty());
		const Point closest = this->closest_point(query);
		return Tr().squared_distance_object()(query, closest);
	}

	// closest point with user-specified hint
	template<typename Tr>
	typename AABB_tree<Tr>::Point_and_primitive_id
		AABB_tree<Tr>::closest_point_and_primitive(const Point& query) const
	{
		CGAL_precondition(!empty());
		return closest_point_and_primitive(query,best_hint(query));
	}

	// closest point with user-specified hint
	template<typename Tr>
	typename AABB_tree<Tr>::Point_and_primitive_id
		AABB_tree<Tr>::closest_point_and_primitive(const Point& query,
		const Point_and_primitive_id& hint) const
	{
		CGAL_precondition(!empty());
    using namespace CGAL::internal::AABB_tree;
    typedef typename AABB_tree<Tr>::AABB_traits AABBTraits;
		Projection_traits<AABBTraits> projection_traits(hint.first,hint.second,m_traits);
		this->traversal(query, projection_traits);
		return projection_traits.closest_point_and_primitive();
	}

} // end namespace CGAL

#include <CGAL/internal/AABB_tree/AABB_ray_intersection.h>

#include <CGAL/enable_warnings.h>

#endif // CGAL_AABB_TREE_H

/***EMACS SETTINGS**    */
/* Local Variables:     */
/* tab-width: 2         */
/* indent-tabs-mode: t  */
/* End:                 */
