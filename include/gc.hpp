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
void										mark_and_sweep();

typedef std::set<base_handle*, std::less<base_handle*> >			handle_ptr_set;

struct block_data__
{
	size_t									count;	///< number of handles pointing to this block (faster destruction)
	handle_ptr_set							refs;	///< references inside this block

	block_data__() : count(0)	{}
	block_data__(const block_data__& bd) : count(bd.count), refs(bd.refs)	{}
};

typedef std::map<block, block_data__, std::less<block> >			block_map;

/**
  block
  */
struct block
{
	block(void* s, void* e) : range_(s, e)
	{

	}

	block(const block& b) : range_(b.range_)
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


	static block_map						gc_blocks_;
	static handle_ptr_set					root_refs_;

	static size_t							total_size_;

	static block_map::iterator				find_address_block(void *addr);

	friend void								mark_and_sweep();

	friend struct							base_handle;
	template<typename T> friend struct		handle;
	template<typename T> friend class		container_allocator;
	friend class							object;
};	// struct block

class object
{
public:
	object()
	{
		block_iter__	= block::find_address_block(this);
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

		block::gc_blocks_[block(start, end)]	= block_data__();
#ifdef GC_DEBUG
		std::cout << "block from: " << std::hex << (size_t)(start) << " to " << (size_t)(end) << std::endl;
#endif
		block::total_size_	+= s;
		return start;
	}

	void	operator delete(void* p)
	{
		block_map::iterator	it	= block::find_address_block(p);

		if( it == block::gc_blocks_.end() )
		{
#ifdef GC_DEBUG
			std::cout << "ERROR: attempt to free a non GC block with delete" << std::endl;
#endif
			return;
		}

		block::total_size_	-= reinterpret_cast<size_t>(it->first.range_.second) - reinterpret_cast<size_t>(it->first.range_.first) + 1;
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
private:
	block_map::iterator		block_iter__;
	template<typename T> friend struct		handle;
};

///////////////////////////////////////////////////////////////////////////////

struct base_handle
{
protected:
	block_map::iterator					block_iter_;	// TODO: this is dangerous: need to use only blocks, or at least mark them.
	//		SHOULD NEVER use static data, because, it might revive if someone
	//		creates an object while this is destroying a cycle object

	void								delete_if_filled(void *ref)
	{
		if( ref && block::gc_blocks_.find(block(ref, ref)) != block::gc_blocks_.end() )
		{
			if( !block_iter_->second.count )
			{
				--(block_iter_->second.count);
				if( !block_iter_->second.count )
				{
#ifdef GC_DEBUG
					std::cout << "delete object from handle << " << std::hex << (size_t)(this) << std::endl;
#endif
					object*	o	= static_cast<object*>(block_iter_->first.range_.first);
					delete o;
				}
			}
		}
	}

	friend struct						block;
	friend void							mark_and_sweep();
};


template<typename T>
struct handle : public base_handle
{
	handle() : base_handle(), ref_(0)
	{
		internal_init_();
		block_iter_	= block::gc_blocks_.end();
#ifdef GC_DEBUG
		std::cout << "handle << " << std::hex << (size_t)(this) << std::endl;
#endif
	}

	handle(T* t) : ref_(t)
	{
		internal_init_();
		block_iter_ = static_cast<const object*>(ref_)->block_iter__;
		++(block_iter_->second.count);

#ifdef GC_DEBUG
		std::cout << "handle << " << std::hex << (size_t)(this) << std::endl;
#endif
	}

	handle(const handle& r) : ref_(r.ref_)
	{
		internal_init_();
		block_iter_	= r.block_iter_;
		++(block_iter_->second.count);
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
	~handle()
	{
		delete_if_filled(ref_);

		// check if we belong to some block
		block_map::iterator	it = block::find_address_block(this);
		if( it != block::gc_blocks_.end() )
		{
			// this belongs to an object that is managed by GC
			handle_ptr_set& refs	= const_cast<handle_ptr_set&>(it->second.refs);
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

	handle& operator = (const handle& r)
	{
		delete_if_filled(ref_);
		ref_	= r.ref_;
		block_iter_	= r.block_iter_;
		++(block_iter_->second.count);
		return *this;
	}

	handle& operator = (const T* r)
	{
		delete_if_filled(ref_);
		block_iter_ = static_cast<const object*>(r)->block_iter__;
		++(block_iter_->second.count);
		ref_	= r;

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
		block_map::iterator	it = block::find_address_block(this);
		if( it != block::gc_blocks_.end() )
		{
			// this belongs to an object that is managed by GC
#ifdef GC_DEBUG
			std::cout << "child: block[" << std::hex << (size_t)(*it).first.range_.first << "]->gc_ref" << std::endl;
#endif
			handle_ptr_set&	refs	= const_cast<handle_ptr_set&>((*it).second.refs);

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
		std::cout << "internal_init: block_refs = " << (*it).second.refs.size() << std::endl;
#endif
	}

	friend struct			block;
};




}	// namespace gc

#endif // GC_HPP
