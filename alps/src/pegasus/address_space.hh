/* 
 * (c) Copyright 2016 Hewlett Packard Enterprise Development LP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ALPS_PEGASUS_ADDRESS_SPACE_HH_
#define _ALPS_PEGASUS_ADDRESS_SPACE_HH_

#include <sys/types.h>
#include <map>
#include <vector>

#include "common/error_code.hh"
#include "pegasus/address_space_options.hh"
#include "pegasus/pegasthread.hh"
#include "pegasus/pegasus_options.hh"
#include "pegasus/region_file.hh"
#include "pegasus/mm.hh"

namespace alps {


// For the current LFS and Machine, failure and interleave groups are the same?
// failure group: the set of nodes affected by the failure of a single component (e.g. data center outage, top-of-the rack switch, interleave group, SOC)
// other plausible alternative definitions:
// - failure group: set of interleave groups affected by the failure of a single component
// - failure group: set of nodes affected by the failure of a single interleave group
// 
// the primary definition makes sense in a shared-nothing architecture where 
// nodes provide access to your data. Losing a node means you lose the ability 
// to access the node's partition (or replica). But in a shared disk system, 
// losing a node doesn't mean you lose the stable data (but you do lose the 
// data cached in SOC (DRAM or cpu cache)). What does it mean for a node to fail in 
// a shared disk? 
// the alternative definitions is perhaps not a good definition as it assumes 
// that failures happen only due to interleave groups failing. but from the 
// memory allocator's point of view things fail because NVM fails? similar to 
// a storage system failing because disk fails (but could fail because memory fails too, right?)
class FailureGroup {


};


// forward declarations
class Config;

/**
 * @brief Persistent global address space. 
 *
 * @details
 * Represents a PErsistent Global Address Space (PEGAS) where region files can 
 * be bound and mapped to. There should be a single PEGAS object instance per 
 * process even though nothing prevents a user from having multiple instances.
 *
 * Binding and mapping a region file to a global address space results in a 
 * region. The global address space supports different modes of region mappings 
 * to allow users select different performance and flexibility tradeoffs. 
 * These modes are supported through template polymorphism rather than virtual 
 * polymorphish to reduce the runtime overhead for accessing regions.
 * 
 * Mapping modes include:
 * - single segment direct relocatable mapping, where the whole region is 
 *   directly mapped to an arbitraty address. This mode enables using 
 *   relative offset pointers (similar to offset_ptr) for flexible mapping 
 *   and addressing at the expense of performance (i.e., some overhead to perform 
 *   simple pointer arithmetic relative to the base of region). 
 *
 * Mapping modes that we plan to implement include:
 * - single segment direct fixed mapping, where the whole region is directly 
 *   mapped to a fixed address. This mode allows using virtual addresses as 
 *   pointers for low-overhead addressing at the expense of flexibility.
 * - multiple segments direct mapping, where the region is segmented into
 *   multiple mappings. A base table translation is used for translation, which 
 *   introduces extra overhead due to addition and masking instructions. However, 
 *   there is more flexibility when mapping large regions as the region does not 
 *   need to be memory mapped contiguously.
 *
 * @openquestion 
 * Do we need to identify the global address space using a context identifier
 * similarly to traditional PGAS and RDMA? The lack of a context identifier
 * makes the global address space stateless, which simplifies failure handling:
 * a process can resume a global address space by memory mapping back the 
 * region files it needs (in effect, the region files are used as an identifier).
 * Altough resuming a global address space that maps a single region file appears
 * to be well handled by this approach, resuming a global address space with
 * multiple regions can be more complicated as a process has to identify all
 * the region files comprising a global address space. A context identifier 
 * could simplify this by identifying the set of region files. In turn, the 
 * set itself could be stored in a highly available store such as Raft or 
 * Zookeeper.
 *
 * @openquestion 
 * Should we support pointers between persistent regions? The current approach 
 * is to support this through special Zeta Pointers. However, we don't have a
 * good solution on how to ensure that the closure of a region is available and 
 * mapped.
 *
 */
class AddressSpace {
public:
    AddressSpace(AddressSpaceOptions& address_space_options);

    ErrorCode init();

     /**
     * @brief Binds and maps a region file into the global address space.
     *
     * @param[in] region_file The region file to map into the global address space.
     * @param[out] pregion An object representing the region of the global address 
     * space where the file is mapped to. 
     */
    template<class RegionT>
    ErrorCode map(RegionFile* region_file, RegionT** pregion);

    /**
     * @brief Unmaps a previously mapped region.
     *
     * @param[in] region The region to unmap.
     */
    template<class RegionT>
    ErrorCode unmap(RegionT* region);

    /**
     * @brief Returns region identified by region identifier \a region_id.
     *
     * @param[in] region_id Region identifier 
     */
    Region* region(RegionId region_id);

    /**
     * @brief Performs reverse translation of a virtual address.
     *
     * @param[in] vaddr The virtual address to reverse translate.
     * @param[out] pregion The region the virtual address is mapped to.
     * @param[out] offset The offset relative to the region base.
     */
    ErrorCode rtrans(void* vaddr, Region** pregion, LinearAddr* offset);

    MemoryManager* mm();

//private:
    AddressSpaceOptions              address_space_options_; ///< Address space runtime options
    MemoryManager*                   mm_; ///< Memory manager mapping regions into virtual memory
    std::multimap<RegionId, Region*> regions_; ///< A table mapping region_id to region
};

inline ErrorCode AddressSpace::rtrans(void* vaddr, Region** pregion, LinearAddr* offset)
{
    return mm_->rtrans(vaddr, pregion, offset);
}

inline MemoryManager* AddressSpace::mm()
{
    return mm_;
}


} // namespace alps

#endif // _ALPS_PEGASUS_ADDRESS_SPACE_HH_
