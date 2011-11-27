/*
 C++ Garbage Collector,

 Copyright (c) 2011 by Wael El Oraiby
 All rights reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//
// C++ Mark and Sweep Garbage Collector design pattern helper classes.
//
// This is a sample implementation of a mark and sweep garbage collected classes
// mechanism. It involves managing memory as blocks and instancing classes as
// garbage collected objects.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef GC_HPP
#define GC_HPP

#include <memory>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <limits>

#include <malloc.h>

// enable/disable extensive debugging log
#define GC_DEBUG

#ifdef GC_DEBUG
#include <iostream>
#endif

namespace gc
{

struct base_handle;
struct block;
class object;
template<typename T> class			allocator_;
template<typename T> class			container_allocator;
void								mark_and_sweep();

template<typename T>
class allocator_
{
public :
	//    typedefs

	typedef T value_type;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

public :
	//    convert an allocator<T> to allocator<U>

	template<typename U>
	struct rebind
	{
		typedef allocator_<U> other;
	};

public :
	inline explicit allocator_() {}

	inline ~allocator_() {}

	inline explicit allocator_(allocator_ const&) {}

	template<typename U>
	inline allocator_(allocator_<U> const&) {}

	//    address

	inline pointer address(reference r) { return &r; }

	inline const_pointer address(const_reference r) { return &r; }

	//    memory allocation

	inline pointer allocate(size_type cnt, typename std::allocator<void>::const_pointer = 0)
	{
		return reinterpret_cast<pointer>(malloc(cnt * sizeof(T)));
	}

	inline void deallocate(pointer p, size_type)
	{
		free(p);
	}

	//    size

	inline size_type max_size() const
	{
		return std::numeric_limits<size_type>::max() / sizeof(T);
	}

	//    construction/destruction

	inline void construct(pointer p, const T& t) { new(p) T(t); }

	inline void destroy(pointer p) { p->~T(); }

	inline bool operator == (allocator_ const&) { return true; }

	inline bool operator != (allocator_ const& a) { return !operator==(a); }
};    //    end of class Allocator

typedef std::set<block, std::less<block>, allocator_<block> >							block_set;
typedef std::set<base_handle*, std::less<base_handle*>, allocator_<base_handle*> >		handle_ptr_set;

/**
  block
  */
struct block
{
	block(void* s, void* e, bool ia) : range_(s, e), is_container_(ia)
	{

	}

	block(const block& b) : range_(b.range_), is_container_(b.is_container_), count_(b.count_), refs_(b.refs_)
	{

	}

	// this is for splay_set
	friend bool			operator < (const block& p0, const block& p1)
	{
		size_t		p0_addr_s	= reinterpret_cast<size_t>(p0.range_.first);
		size_t		p0_addr_e	= reinterpret_cast<size_t>(p0.range_.second);

		size_t		p1_addr_s	= reinterpret_cast<size_t>(p1.range_.first);
		size_t		p1_addr_e	= reinterpret_cast<size_t>(p1.range_.second);

		if(( p0_addr_s >= p1_addr_s && p0_addr_s <= p1_addr_e )
		   || ( p1_addr_s >= p0_addr_s && p1_addr_s <= p0_addr_e ))
			return false;
		return (p0_addr_s < p1_addr_s);
	}


protected:
	std::pair<void*, void*>					range_;
	bool									is_container_ : 1;

	size_t									count_;	///< number of handles pointing to this block (faster destruction)
	handle_ptr_set							refs_;	///< references inside this block

	static block_set						gc_blocks_;
	static handle_ptr_set					root_refs_;

	static size_t							total_size_;

	static block_set::iterator				find_address_block(void *addr);

	friend void								mark_and_sweep();

	template<typename T> friend struct		generic_handle;
	template<typename T> friend class		container_allocator;
	friend class							object;
};	// struct block


///////////////////////////////////////////////////////////////////////////////

struct base_handle
{
protected:
	block_set::iterator					block_iter_;

	friend struct						block;
	friend void							mark_and_sweep();
};


template<typename T>
struct generic_handle : public base_handle
{
	generic_handle() : base_handle(), ref_(0)
	{
		internal_init_();
		block_iter_	= block::gc_blocks_.end();
#ifdef GC_DEBUG
		std::cout << "handle << " << std::hex << (size_t)(this) << std::endl;
#endif
	}

	generic_handle(T* t) : ref_(t)
	{
		internal_init_();
		block_iter_ = block::find_address_block(ref_);

#ifdef GC_DEBUG
		std::cout << "handle << " << std::hex << (size_t)(this) << std::endl;
#endif
	}

	generic_handle(const generic_handle& r) : ref_(r.ref_)
	{
		internal_init_();
		block_iter_	= r.block_iter_;
#ifdef GC_DEBUG
		std::cout << "handle << " << std::hex << (size_t)(this) << std::endl;
#endif
	}

	//
	// delete order:
	//
	// 1 - if root ref, delete is called, no problem
	//
	// 2 - if child ref, the block is freed after all refs are destroyed
	//
	~generic_handle()
	{
		// check if we belong to some block
		block_set::iterator	it = block::find_address_block(this);
		if( it != block::gc_blocks_.end() )
		{
			// this belongs to an object that is managed by GC
			handle_ptr_set& refs	= const_cast<handle_ptr_set&>(it->refs_);
			refs.erase(this);
#ifdef GC_DEBUG
			std::cout << "X - block_ref" << std::endl;
#endif
		}
		else
		{
			// belongs to an object that is not managed by GC (root)
#ifdef GC_DEBUG
			std::cout << "X - root_ref" << std::endl;
#endif
			block::root_refs_.erase(this);
		}
#ifdef GC_DEBUG
		std::cout << "#root_refs: " << std::dec << block::root_refs_.size() << std::endl;
#endif
	}

	generic_handle& operator = (const generic_handle& r)
	{
		ref_	= r.ref_;
		block_iter_	= r.block_iter_;
		return *this;
	}

	T& operator * ()
	{
		return *ref_;
	}

	T* operator -> ()
	{
		return ref_;
	}

	T* get_ref() const
	{
		return ref_;
	}

protected:
	T*			ref_;

	void		internal_init_()
	{
		// check if we belong to some block
		block_set::iterator	it = block::find_address_block(this);
		if( it != block::gc_blocks_.end() )
		{
			// this belongs to an object that is managed by GC
#ifdef GC_DEBUG
			std::cout << "child: block[" << std::hex << (size_t)(*it).range_.first << "]->gc_ref" << std::endl;
#endif
			handle_ptr_set&	refs	= const_cast<handle_ptr_set&>((*it).refs_);

			handle_ptr_set::iterator bitr = refs.find(this);
			if( bitr == refs.end() )
			{
#ifdef GC_DEBUG
				std::cout << "adding a block ref: " << std::endl;
#endif
			}
			refs.insert(this);
		}
		else
		{
			// belongs to an object that is not managed by GC (root)
			block::root_refs_.insert(this);
#ifdef GC_DEBUG
			std::cout << "root: gc_ref->block" << std::endl;
#endif
		}

#ifdef GC_DEBUG
		std::cout << "internal_init: block_refs = " << (*it).refs_.size() << std::endl;
#endif
	}

	friend struct			block;
};

class object
{
public:
	object()
	{
#ifdef GC_DEBUG
		std::cout << "gc::object" << std::endl;
#endif
	}

	virtual ~object()
	{
#ifdef GC_DEBUG
		std::cout << "~gc::object" << std::endl;
#endif
	}

	void*	operator new(size_t s)
	{
		//void*::operator new()
		unsigned char* start	= static_cast<unsigned char*>(malloc(s));
		unsigned char* end		= &(start[s - 1]);

		block::gc_blocks_.insert(block(start, end, false));
#ifdef GC_DEBUG
		std::cout << "block from: " << std::hex << (size_t)(start) << " to " << (size_t)(end) << std::endl;
#endif
		block::total_size_	+= s;
		return start;
	}

	void	operator delete(void* p)
	{
		block_set::iterator	it	= block::find_address_block(p);

		if( it == block::gc_blocks_.end() )
		{
#ifdef GC_DEBUG
			std::cout << "ERROR: attempt to free a non GC block with delete" << std::endl;
#endif
			return;
		}

		const block &b	= *it;
		block::total_size_	-= reinterpret_cast<size_t>(b.range_.second) - reinterpret_cast<size_t>(b.range_.first) + 1;
		block::gc_blocks_.erase(it);

		free(p);

#ifdef GC_DEBUG
		std::cout << "deleting object" << std::endl;
#endif
	}

	void*	operator new[](size_t) throw()
	{
		return NULL;
	}

	void	operator delete[](void*)
	{
	}
};

////////////////////////////////////////////////////////////////////////////////
// adding container types to garbage collection
////////////////////////////////////////////////////////////////////////////////
template<typename T>
class container_allocator {
public :
	//    typedefs

	typedef T value_type;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

public :
	//    convert an allocator<T> to allocator<U>

	template<typename U>
	struct rebind
	{
		typedef container_allocator<U> other;
	};

public :
	inline explicit container_allocator() {}

	inline ~container_allocator() {}

	inline explicit container_allocator(container_allocator const&) {}

	template<typename U>
	inline container_allocator(container_allocator<U> const&) {}

	//    address

	inline pointer address(reference r) { return &r; }

	inline const_pointer address(const_reference r) { return &r; }

	//    memory allocation

	inline pointer allocate(size_type cnt, typename std::allocator<void>::const_pointer = 0)
	{
		size_t				s	= cnt * sizeof(T);
		unsigned char* start	= static_cast<unsigned char*>(malloc(s));
		unsigned char* end		= &(start[s - 1]);

		block::gc_blocks_.insert(block(start, end, true));
#ifdef GC_DEBUG
		std::cout << "allocator:block from: " << std::hex << (size_t)(start) << " to " << (size_t)(end) << std::endl;
#endif
		return reinterpret_cast<pointer>(start);
	}

	inline void deallocate(pointer p, size_type)
	{
		block_set::iterator	it	= block::find_address_block(p);

		if( it == block::gc_blocks_.end() )
		{
#ifdef GC_DEBUG
			std::cout << "allocator:ERROR: attempt to free a non GC block with delete" << std::endl;
#endif
			return;
		}

		block::gc_blocks_.erase(it);

		free(p);

#ifdef GC_DEBUG
		std::cout << "allocator:deleting object" << std::endl;
#endif
	}

	//    size

	inline size_type max_size() const
	{
		return std::numeric_limits<size_type>::max() / sizeof(T);
	}

	//    construction/destruction

	inline void construct(pointer p, const T& t) { new(p) T(t); }

	inline void destroy(pointer p) { p->~T(); }

	inline bool operator == (container_allocator const&) { return true; }

	inline bool operator != (container_allocator const& a) { return !operator==(a); }
};    //    end of class Allocator

template<typename T>
class vector : public std::vector< T, container_allocator<T> >
{
};

template<typename T>
class list : public std::list< T, container_allocator<T> >
{
};

template < class T, class Compare = std::less<T> >
class set : public std::set< T, Compare, container_allocator<T> >
{
};

template < class Key, class T, class Compare = std::less<Key> >
class map : public std::map< Key, T, Compare, container_allocator<std::pair<const Key,T> > >
{
};

}	// namespace gc

#endif // GC_HPP
