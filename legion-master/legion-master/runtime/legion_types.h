/* Copyright 2015 Stanford University
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


#ifndef __LEGION_TYPES_H__
#define __LEGION_TYPES_H__

/**
 * \file legion_types.h
 */

#include "legion_config.h"
#include "lowlevel.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <stdint.h>

#include "limits.h"

#include <map>
#include <set>
#include <list>
#include <deque>
#include <vector>

namespace BindingLib { class Utility; } // BindingLib namespace

namespace LegionRuntime {
  /**
   * \struct LegionStaticAssert
   * Help with static assertions.
   */
  template<bool> struct LegionStaticAssert;
  template<> struct LegionStaticAssert<true> { };
#define LEGION_STATIC_ASSERT(condition) \
  do { LegionStaticAssert<(condition)>(); } while (0)

  /**
   * \struct LegionTypeEquality
   * Help with checking equality of types.
   */
  template<typename T, typename U>
  struct LegionTypeInequality {
  public:
    static const bool value = true;
  };
  template<typename T>
  struct LegionTypeInequality<T,T> {
  public:
    static const bool value = false;
  };
  
  namespace HighLevel {

    typedef ::legion_error_t LegionErrorType;
    typedef ::legion_privilege_mode_t PrivilegeMode;
    typedef ::legion_allocate_mode_t AllocateMode;
    typedef ::legion_coherence_property_t CoherenceProperty;
    typedef ::legion_region_flags_t RegionFlags;
    typedef ::legion_handle_type_t HandleType;
    typedef ::legion_dependence_type_t DependenceType;

    enum OpenState {
      NOT_OPEN            = 0,
      OPEN_READ_ONLY      = 1,
      OPEN_READ_WRITE     = 2, // unknown dirty information below
      OPEN_SINGLE_REDUCE  = 3, // only one open child with reductions below
      OPEN_MULTI_REDUCE   = 4, // multiple open children with same reduction
    };

    // Runtime task numbering 
    enum {
      INIT_FUNC_ID          = LowLevel::Processor::TASK_ID_PROCESSOR_INIT,
      SHUTDOWN_FUNC_ID      = LowLevel::Processor::TASK_ID_PROCESSOR_SHUTDOWN,
      HLR_TASK_ID           = LowLevel::Processor::TASK_ID_FIRST_AVAILABLE,
      TASK_ID_AVAILABLE     = (LowLevel::Processor::TASK_ID_FIRST_AVAILABLE+1),
    };

    // redop IDs - none used in HLR right now, but 0 isn't allowed
    enum {
      REDOP_ID_AVAILABLE    = 1,
    };

    // Enumeration of high-level runtime tasks
    enum HLRTaskID {
      HLR_SCHEDULER_ID,
      HLR_MESSAGE_ID,
      HLR_POST_END_ID,
      HLR_DEFERRED_MAPPING_ID,
      HLR_DEFERRED_COMPLETE_ID,
      HLR_RECLAIM_LOCAL_FIELD_ID,
      HLR_DEFERRED_COLLECT_ID,
      HLR_TRIGGER_DEPENDENCE_ID,
      HLR_TRIGGER_OP_ID,
      HLR_TRIGGER_TASK_ID,
      HLR_DEFERRED_RECYCLE_ID,
      HLR_DEFERRED_SLICE_ID,
      HLR_MUST_INDIV_ID,
      HLR_MUST_INDEX_ID,
      HLR_MUST_MAP_ID,
      HLR_MUST_DIST_ID,
      HLR_MUST_LAUNCH_ID,
      HLR_DEFERRED_FUTURE_SET_ID,
      HLR_DEFERRED_FUTURE_MAP_SET_ID,
      HLR_RESOLVE_FUTURE_PRED_ID,
      HLR_MPI_RANK_ID,
      HLR_CONTRIBUTE_COLLECTIVE_ID,
      HLR_CHECK_STATE_ID,
      HLR_MAPPER_TASK_ID,
    };

    // Forward declarations for user level objects
    // legion.h
    class FieldSpace;
    class LogicalRegion;
    class LogicalPartition;
    class IndexAllocator;
    class FieldAllocator;
    class TaskArgument;
    class ArgumentMap;
    class Lock;
    struct LockRequest;
    class Grant;
    class PhaseBarrier;
    struct RegionRequirement;
    struct IndexSpaceRequirement;
    struct FieldSpaceRequirement;
    struct TaskLauncher;
    struct IndexLauncher;
    struct InlineLauncher;
    struct CopyLauncher;
    struct AcquireLauncher;
    struct ReleaseLauncher;
    class Future;
    class FutureMap;
    class Predicate;
    class PhysicalRegion;
    class IndexIterator;
    class Mappable;
    class Task;
    class Copy;
    class Inline;
    class Acquire;
    class Release;
    class TaskVariantCollection;
    class Mapper; 
    template<typename T> struct ColoredPoints; 
    struct InputArgs;
    class ProjectionFunctor;
    class HighLevelRuntime;

    // Forward declarations for compiler level objects
    // legion.h
    class ColoringSerializer;
    class DomainColoringSerializer;

    // Forward declarations for wrapper tasks
    // legion.h
    class LegionTaskWrapper;
    class LegionSerialization;

    // Forward declarations for C wrapper objects
    // legion_c_util.h
    class CObjectWrapper;

    // Forward declarations for runtime level objects
    // runtime.h
    class Collectable;
    class ArgumentMapStore;
    class ProcessorManager;
    class MessageManager;
    class GarbageCollectionEpoch;
    class Runtime;

    // legion_ops.h
    class Operation;
    class SpeculativeOp;
    class MapOp;
    class CopyOp;
    class FenceOp;
    class FrameOp;
    class DeletionOp;
    class CloseOp;
    class InterCloseOp;
    class PostCloseOp;
    class AcquireOp;
    class ReleaseOp;
    class DynamicCollectiveOp;
    class FuturePredOp;
    class NotPredOp;
    class AndPredOp;
    class OrPredOp;
    class MustEpochOp;
    class TaskOp;

    // legion_tasks.h
    class SingleTask;
    class MultiTask;
    class IndividualTask;
    class PointTask;
    class WrapperTask;
    class RemoteTask;
    class InlineTask;
    class IndexTask;
    class SliceTask;
    class RemoteTask;
    
    // legion_trace.h
    class LegionTrace;
    class TraceCaptureOp;
    class TraceCompleteOp;

    // region_tree.h
    class RegionTreeForest;
    class StateDirectory;
    class IndexTreeNode;
    class IndexSpaceNode;
    class IndexPartNode;
    class FieldSpaceNode;
    class RegionTreeNode;
    class RegionNode;
    class PartitionNode;

    class RegionTreeContext;
    class RegionTreePath;
    class PathTraverser;
    class NodeTraverser;
    class PremapTraverser;
    class MappingTraverser;

    struct LogicalState;
    class PhysicalVersion;

    class DistributedCollectable;
    class HierarchicalCollectable;
    class LayoutDescription;
    class PhysicalManager; // base class for instance and reduction
    class LogicalView; // base class for instance and reduction
    class InstanceManager;
    class InstanceKey;
    class InstanceView;
    class MaterializedView;
    class CompositeView;
    class CompositeNode;
    class MappingRef;
    class InstanceRef;
    class InnerTaskView;
    class ReductionManager;
    class ListReductionManager;
    class FoldReductionManager;
    class ReductionView;

    class RegionAnalyzer;
    class RegionMapper;

    struct RegionUsage;

    struct EscapedUser;
    struct EscapedCopy;
    struct GenericUser;
    struct LogicalUser;
    struct PhysicalUser;
    struct TraceInfo;
    struct LogicalCloser;
    struct PhysicalCloser;
    class ReductionCloser;
    class TreeCloseImpl;
    class TreeClose;
    struct CloseInfo;

    // legion_utilities.h
    class Serializer;
    class Deserializer;
    template<typename T> class Fraction;
    template<typename T, unsigned int MAX, 
             unsigned SHIFT, unsigned MASK> class BitMask;
    template<typename T, unsigned int MAX,
             unsigned SHIFT, unsigned MASK> class TLBitMask;
#ifdef __SSE2__
    template<unsigned int MAX> class SSEBitMask;
    template<unsigned int MAX> class SSETLBitMask;
#endif
#ifdef __AVX__
    template<unsigned int MAX> class AVXBitMask;
    template<unsigned int MAX> class AVXTLBitMask;
#endif
    template<typename T, unsigned LOG2MAX> class BitPermutation;
    template<typename IT, typename DT, bool BIDIR = false> class IntegerSet;

    // legion_logging.h
    class TreeStateLogger;

    typedef LowLevel::Runtime LLRuntime;
    typedef LowLevel::Machine Machine;
    typedef LowLevel::Domain Domain;
    typedef LowLevel::DomainPoint DomainPoint;
    typedef LowLevel::IndexSpace IndexSpace;
    typedef LowLevel::IndexSpaceAllocator IndexSpaceAllocator;
    typedef LowLevel::RegionInstance PhysicalInstance;
    typedef LowLevel::Memory Memory;
    typedef LowLevel::Processor Processor;
    typedef LowLevel::Event Event;
    typedef LowLevel::Event MapperEvent;
    typedef LowLevel::UserEvent UserEvent;
    typedef LowLevel::Reservation Reservation;
    typedef LowLevel::Barrier Barrier;
    typedef ::legion_reduction_op_id_t ReductionOpID;
    typedef LowLevel::ReductionOpUntyped ReductionOp;
    typedef LowLevel::Machine::ProcessorMemoryAffinity ProcessorMemoryAffinity;
    typedef LowLevel::Machine::MemoryMemoryAffinity MemoryMemoryAffinity;
    typedef LowLevel::ElementMask::Enumerator Enumerator;
    typedef ::legion_address_space_t AddressSpace;
    typedef ::legion_task_priority_t TaskPriority;
    typedef ::legion_color_t Color;
    typedef ::legion_index_partition_t IndexPartition;
    typedef ::legion_field_id_t FieldID;
    typedef ::legion_trace_id_t TraceID;
    typedef ::legion_mapper_id_t MapperID;
    typedef ::legion_context_id_t ContextID;
    typedef ::legion_instance_id_t InstanceID;
    typedef ::legion_field_space_id_t FieldSpaceID;
    typedef ::legion_generation_id_t GenerationID;
    typedef ::legion_type_handle TypeHandle;
    typedef ::legion_projection_id_t ProjectionID;
    typedef ::legion_region_tree_id_t RegionTreeID;
    typedef ::legion_distributed_id_t DistributedID;
    typedef ::legion_address_space_id_t AddressSpaceID;
    typedef ::legion_tunable_id_t TunableID;
    typedef ::legion_mapping_tag_id_t MappingTagID;
    typedef ::legion_semantic_tag_t SemanticTag;
    typedef ::legion_variant_id_t VariantID;
    typedef ::legion_unique_id_t UniqueID;
    typedef ::legion_version_id_t VersionID;
    typedef ::legion_task_id_t TaskID;
    typedef SingleTask* Context;
    typedef std::map<Color,ColoredPoints<ptr_t> > Coloring;
    typedef std::map<Color,Domain> DomainColoring;
    typedef std::map<Color,std::set<Domain> > MultiDomainColoring;
    typedef void (*RegistrationCallbackFnptr)(Machine machine, 
        HighLevelRuntime *rt, const std::set<Processor> &local_procs);
    typedef LogicalRegion (*RegionProjectionFnptr)(LogicalRegion parent, 
        const DomainPoint&, HighLevelRuntime *rt);
    typedef LogicalRegion (*PartitionProjectionFnptr)(LogicalPartition parent, 
        const DomainPoint&, HighLevelRuntime *rt);
    typedef bool (*PredicateFnptr)(const void*, size_t, 
        const std::vector<Future> futures);
    typedef std::map<ProjectionID,RegionProjectionFnptr> 
      RegionProjectionTable;
    typedef std::map<ProjectionID,PartitionProjectionFnptr> 
      PartitionProjectionTable;
    typedef void (*LowLevelFnptr)(const void*,size_t,Processor);
    typedef void (*InlineFnptr)(const Task*,const std::vector<PhysicalRegion>&,
      Context,HighLevelRuntime*,void*&,size_t&);
    // A little bit of logic here to figure out the 
    // kind of bit mask to use for FieldMask

// The folowing macros are used in the FieldMask instantiation of BitMask
// If you change one you probably have to change the others too
#define FIELD_TYPE          uint64_t 
#define FIELD_SHIFT         6
#define FIELD_MASK          0x3F
#define FIELD_ALL_ONES      0xFFFFFFFFFFFFFFFF

#if defined(__AVX__)
#if (MAX_FIELDS > 256)
    typedef AVXTLBitMask<MAX_FIELDS> FieldMask;
#elif (MAX_FIELDS > 128)
    typedef AVXBitMask<MAX_FIELDS> FieldMask;
#elif (MAX_FIELDS > 64)
    typedef SSEBitMask<MAX_FIELDS> FieldMask;
#else
    typedef BitMask<FIELD_TYPE,MAX_FIELDS,FIELD_SHIFT,FIELD_MASK> FieldMask;
#endif
#elif defined(__SSE2__)
#if (MAX_FIELDS > 128)
    typedef SSETLBitMask<MAX_FIELDS> FieldMask;
#elif (MAX_FIELDS > 64)
    typedef SSEBitMask<MAX_FIELDS> FieldMask;
#else
    typedef BitMask<FIELD_TYPE,MAX_FIELDS,FIELD_SHIFT,FIELD_MASK> FieldMask;
#endif
#else
#if (MAX_FIELDS > 64)
    typedef TLBitMask<FIELD_TYPE,MAX_FIELDS,FIELD_SHIFT,FIELD_MASK> FieldMask;
#else
    typedef BitMask<FIELD_TYPE,MAX_FIELDS,FIELD_SHIFT,FIELD_MASK> FieldMask;
#endif
#endif
    typedef BitPermutation<FieldMask,FIELD_LOG2> FieldPermutation;
    typedef Fraction<unsigned long> InstFrac;
#undef FIELD_SHIFT
#undef FIELD_MASK

    // Similar logic as field masks for node masks

// The following macros are used in the NodeMask instantiation of BitMask
// If you change one you probably have to change the others too
#define NODE_TYPE           uint64_t
#define NODE_SHIFT          6
#define NODE_MASK           0x3F
#define NODE_ALL_ONES       0xFFFFFFFFFFFFFFFF

#if defined(__AVX__)
#if (MAX_NUM_NODES > 256)
    typedef AVXTLBitMask<MAX_NUM_NODES> NodeMask;
#elif (MAX_NUM_NODES > 128)
    typedef AVXBitMask<MAX_NUM_NODES> NodeMask;
#elif (MAX_NUM_NODES > 64)
    typedef SSEBitMask<MAX_NUM_NODES> NodeMask;
#else
    typedef BitMask<NODE_TYPE,MAX_NUM_NODES,NODE_SHIFT,NODE_MASK> NodeMask;
#endif
#elif defined(__SSE2__)
#if (MAX_NUM_NODES > 128)
    typedef SSETLBitMask<MAX_NUM_NODES> NodeMask;
#elif (MAX_NUM_NODES > 64)
    typedef SSEBitMask<MAX_NUM_NODES> NodeMask;
#else
    typedef BitMask<NODE_TYPE,MAX_NUM_NODES,NODE_SHIFT,NODE_MASK> NodeMask;
#endif
#else
#if (MAX_NUM_NODES > 64)
    typedef TLBitMask<NODE_TYPE,MAX_NUM_NODES,NODE_SHIFT,NODE_MASK> NodeMask;
#else
    typedef BitMask<NODE_TYPE,MAX_NUM_NODES,NODE_SHIFT,NODE_MASK> NodeMask;
#endif
#endif
    typedef IntegerSet<AddressSpaceID,NodeMask> NodeSet;

#undef NODE_SHIFT
#undef NODE_MASK

// The following macros are used in the ProcessorMask instantiation of BitMask
// If you change one you probably have to change the others too
#define PROC_TYPE           uint64_t
#define PROC_SHIFT          6
#define PROC_MASK           0x3F
#define PROC_ALL_ONES       0xFFFFFFFFFFFFFFFF

#if defined(__AVX__)
#if (MAX_NUM_PROCS > 256)
    typedef AVXTLBitMask<MAX_NUM_PROCS> ProcessorMask;
#elif (MAX_NUM_PROCS > 128)
    typedef AVXBitMask<MAX_NUM_PROCS> ProcessorMask;
#elif (MAX_NUM_PROCS > 64)
    typedef SSEBitMask<MAX_NUM_PROCS> ProcessorMask;
#else
    typedef BitMask<PROC_TYPE,MAX_NUM_PROCS,PROC_SHIFT,PROC_MASK> ProcessorMask;
#endif
#elif defined(__SSE2__)
#if (MAX_NUM_PROCS > 128)
    typedef SSETLBitMask<MAX_NUM_PROCS> ProcessorMask;
#elif (MAX_NUM_PROCS > 64)
    typedef SSEBitMask<MAX_NUM_PROCS> ProcessorMask;
#else
    typedef BitMask<PROC_TYPE,MAX_NUM_PROCS,PROC_SHIFT,PROC_MASK> ProcessorMask;
#endif
#else
#if (MAX_NUM_PROCS > 64)
    typedef TLBitMask<PROC_TYPE,MAX_NUM_PROCS,PROC_SHIFT,PROC_MASK> 
                                                                  ProcessorMask;
#else
    typedef BitMask<PROC_TYPE,MAX_NUM_PROCS,PROC_SHIFT,PROC_MASK> ProcessorMask;
#endif
#endif

#undef PROC_SHIFT
#undef PROC_MASK

#define FRIEND_ALL_RUNTIME_CLASSES                \
    friend class HighLevelRuntime;                \
    friend class Runtime;                         \
    friend class FuturePredicate;                 \
    friend class NotPredicate;                    \
    friend class AndPredicate;                    \
    friend class OrPredicate;                     \
    friend class ProcessorManager;                \
    friend class Operation;                       \
    friend class SpeculativeOp;                   \
    friend class MapOp;                           \
    friend class CopyOp;                          \
    friend class FenceOp;                         \
    friend class FutureOp;                        \
    friend class DynamicCollectiveOp;             \
    friend class FuturePredOp;                    \
    friend class DeletionOp;                      \
    friend class CloseOp;                         \
    friend class InterCloseOp;                    \
    friend class PostCloseOp;                     \
    friend class AcquireOp;                       \
    friend class ReleaseOp;                       \
    friend class NotPredOp;                       \
    friend class AndPredOp;                       \
    friend class OrPredOp;                        \
    friend class MustEpochOp;                     \
    friend class TaskOp;                          \
    friend class SingleTask;                      \
    friend class MultiTask;                       \
    friend class IndividualTask;                  \
    friend class PointTask;                       \
    friend class IndexTask;                       \
    friend class SliceTask;                       \
    friend class RegionTreeForest;                \
    friend class IndexSpaceNode;                  \
    friend class IndexPartNode;                   \
    friend class FieldSpaceNode;                  \
    friend class RegionTreeNode;                  \
    friend class RegionNode;                      \
    friend class PartitionNode;                   \
    friend class LogicalView;                     \
    friend class InstanceView;                    \
    friend class ReductionView;                   \
    friend class MaterializedView;                \
    friend class CompositeView;                   \
    friend class CompositeNode;                   \
    friend class LayoutDescription;               \
    friend class PhysicalManager;                 \
    friend class InstanceManager;                 \
    friend class ReductionManager;                \
    friend class ListReductionManager;            \
    friend class FoldReductionManager;            \
    friend class TreeStateLogger;                 \
    friend class BindingLib::Utility;             \
    friend class CObjectWrapper;

    // Timing events
    enum {
#ifdef PRECISE_HIGH_LEVEL_TIMING
      TIME_HIGH_LEVEL_CREATE_REGION = 100,
      TIME_HIGH_LEVEL_DESTROY_REGION = 101,
      TIME_HIGH_LEVEL_SMASH_REGION = 102
      TIME_HIGH_LEVEL_JOIN_REGION = 103
      TIME_HIGH_LEVEL_CREATE_PARTITION = 104,
      TIME_HIGH_LEVEL_DESTROY_PARTITION = 105,
      TIME_HIGH_LEVEL_ENQUEUE_TASKS = 106,
      TIME_HIGH_LEVEL_STEAL_REQUEST = 107,
      TIME_HIGH_LEVEL_CHILDREN_MAPPED = 108,
      TIME_HIGH_LEVEL_FINISH_TASK = 109,
      TIME_HIGH_LEVEL_NOTIFY_START = 110,
      TIME_HIGH_LEVEL_NOTIFY_MAPPED = 111,
      TIME_HIGH_LEVEL_NOTIFY_FINISH = 112,
      TIME_HIGH_LEVEL_EXECUTE_TASK = 113,
      TIME_HIGH_LEVEL_SCHEDULER = 114,
      TIME_HIGH_LEVEL_ISSUE_STEAL = 115,
      TIME_HIGH_LEVEL_GET_SUBREGION = 116,
      TIME_HIGH_LEVEL_INLINE_MAP = 117,
      TIME_HIGH_LEVEL_CREATE_INDEX_SPACE = 118,
      TIME_HIGH_LEVEL_DESTROY_INDEX_SPACE = 119,
      TIME_HIGH_LEVEL_CREATE_INDEX_PARTITION = 120,
      TIME_HIGH_LEVEL_DESTROY_INDEX_PARTITION = 121,
      TIME_HIGH_LEVEL_GET_INDEX_PARTITION = 122,
      TIME_HIGH_LEVEL_GET_INDEX_SUBSPACE = 123,
      TIME_HIGH_LEVEL_CREATE_FIELD_SPACE = 124,
      TIME_HIGH_LEVEL_DESTROY_FIELD_SPACE = 125,
      TIME_HIGH_LEVEL_GET_LOGICAL_PARTITION = 126,
      TIME_HIGH_LEVEL_GET_LOGICAL_SUBREGION = 127,
      TIME_HIGH_LEVEL_ALLOCATE_FIELD = 128,
      TIME_HIGH_LEVEL_FREE_FIELD = 129,
#else
      TIME_HIGH_LEVEL_CREATE_REGION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_DESTROY_REGION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_SMASH_REGION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_JOIN_REGION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_CREATE_PARTITION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_DESTROY_PARTITION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_ENQUEUE_TASKS = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_STEAL_REQUEST = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_CHILDREN_MAPPED = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_FINISH_TASK = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_NOTIFY_START = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_NOTIFY_MAPPED = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_NOTIFY_FINISH = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_EXECUTE_TASK = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_SCHEDULER = TIME_HIGH_LEVEL,
      TIME_HIGH_LEVEL_ISSUE_STEAL = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_GET_SUBREGION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_INLINE_MAP = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_CREATE_INDEX_SPACE = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_DESTROY_INDEX_SPACE = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_CREATE_INDEX_PARTITION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_DESTROY_INDEX_PARTITION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_GET_INDEX_PARTITION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_GET_INDEX_SUBSPACE = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_CREATE_FIELD_SPACE = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_DESTROY_FIELD_SPACE = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_GET_LOGICAL_PARTITION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_GET_LOGICAL_SUBREGION = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_ALLOCATE_FIELD = TIME_HIGH_LEVEL, 
      TIME_HIGH_LEVEL_FREE_FIELD = TIME_HIGH_LEVEL, 
#endif
    };

  }; // HighLevel namespace
}; // LegionRuntime namespace

#endif // __LEGION_TYPES_H__
