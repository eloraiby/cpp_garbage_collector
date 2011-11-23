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
#include "gc.hpp"

namespace gc
{

block_set			block::gc_blocks_;
handle_ptr_set		block::root_refs_;
size_t				block::total_size_	= 0;

block_set::iterator block::find_address_block(void *addr)
{
	block_set::iterator it = block::gc_blocks_.find(block(addr, addr, false));
	return it;
}

void mark_and_sweep()
{
	block_set							reachable;
	std::vector<block_set::iterator>	q; // this should be replaced with a flag inside the block (cost would be less)

	// start with root refs
	for(handle_ptr_set::iterator it = block::root_refs_.begin(); it != block::root_refs_.end(); ++it )
	{
#ifdef GC_DEBUG
		std::cout << "adding reachable" << std::endl;
#endif
		if( (*it)->block_iter_ != block::gc_blocks_.end() )
		{
			block_set::iterator	mbi = (*it)->block_iter_;
			if( reachable.find(*mbi) == reachable.end() )
			{
				reachable.insert(*mbi);
				q.push_back(mbi);
			}
		}
	}

#ifdef GC_DEBUG
	std::cout << std::dec << "reachable from root: " << reachable.size() << std::endl;
#endif

	size_t	start	= 0;
	size_t	end		= q.size();
	for( ; start < end; ++start )
	{
		block_set::iterator	it = q[start];
#ifdef GC_DEBUG
		std::cout << "start: " << start << ", end: " << end << std::endl;
#endif
		for(handle_ptr_set::iterator i = (*it).refs_.begin(); i != (*it).refs_.end(); ++i )
		{
#ifdef GC_DEBUG
			std::cout << "adding reachable << " << std::hex << (size_t)(*i) << std::endl;
#endif
			block_set::iterator	mbi = (*i)->block_iter_;
			if( /*block::_gc_blocks.find(*mbi)*/mbi != block::gc_blocks_.end() &&
				reachable.find(*mbi) == reachable.end() )
			{
				reachable.insert(*(mbi));
				q.push_back(mbi);
			}
		}

		end = q.size();
	}

#ifdef GC_DEBUG
	std::cout << std::dec << "total reachables: " << reachable.size() << std::endl;
#endif

	std::vector<block_set::iterator>		unreachable;

	for( block_set::iterator it = block::gc_blocks_.begin(); it != block::gc_blocks_.end(); ++it )
	{
		if( reachable.find(*it) == reachable.end() && !it->is_container )
		{
			unreachable.push_back(it);
		}
	}

#ifdef GC_DEBUG
	std::cout << std::dec << "claiming " << unreachable.size() << " unreachable object" << std::endl;
#endif

	for( size_t i = 0; i < unreachable.size(); ++i )
	{
		block	b	= *(unreachable[i]);

		object*	o	= static_cast<object*>(b.range.first);
		delete o;
	}
#ifdef GC_DEBUG
	std::cout << std::dec << "block count " << block::gc_blocks_.size() << std::endl;
	std::cout << std::dec << "total size  " << block::total_size_ << std::endl;
#endif
}

}	// namespace gc
