/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    The source code contained or described herein and all documents related
    to the source code ("Material") are owned by Intel Corporation or its
    suppliers or licensors.  Title to the Material remains with Intel
    Corporation or its suppliers and licensors.  The Material is protected
    by worldwide copyright laws and treaty provisions.  No part of the
    Material may be used, copied, reproduced, modified, published, uploaded,
    posted, transmitted, distributed, or disclosed in any way without
    Intel's prior express written permission.

    No license under any patent, copyright, trade secret or other
    intellectual property right is granted to or conferred upon you by
    disclosure or delivery of the Materials, either expressly, by
    implication, inducement, estoppel or otherwise.  Any license under such
    intellectual property rights must be express and approved by Intel in
    writing.
*/

#include "tbb/spin_mutex.h"
#include "tbb/queuing_mutex.h"
#include "tbb/queuing_rw_mutex.h"
#include "tbb/spin_rw_mutex.h"
#include "tbb/tick_count.h"
#include "tbb/atomic.h"

#include "harness.h"

// This test deliberately avoids a "using tbb" statement,
// so that the error of putting types in the wrong namespace will be caught.

template<typename M>
struct Counter {
    typedef M mutex_type;
    M mutex;
    volatile long value; 
    void flog_once( size_t mode );
};

template<typename M>
void Counter<M>::flog_once(size_t mode)
/** Increments counter once for each iteration in the iteration space. */
{
    if( mode&1 ) {
        // Try implicit acquire and explicit release
        typename mutex_type::scoped_lock lock(mutex);
        value = value+1;
        lock.release();
    } else {
        // Try explicit acquire and implicit release
        typename mutex_type::scoped_lock lock;
        lock.acquire(mutex);
        value = value+1;
    }
}

template<typename M, long N>
struct Invariant {
    typedef M mutex_type;
    M mutex;
    const char* mutex_name;
    volatile long value[N];
    volatile long single_value;
    Invariant( const char* mutex_name_ ) :
        mutex_name(mutex_name_)
    {
        single_value = 0;
        for( long k=0; k<N; ++k )
            value[k] = 0;
    }
    void update() {
        for( long k=0; k<N; ++k )
            ++value[k];
    }
    bool value_is( long expected_value ) const {
        long tmp;
        for( long k=0; k<N; ++k )
            if( (tmp=value[k])!=expected_value ) {
                REPORT("ERROR: %ld!=%ld\n", tmp, expected_value);
                return false;
            }
        return true;
    }
    bool is_okay() {
        return value_is( value[0] );
    }
    void flog_once( size_t mode ); 
};

template<typename M, long N>
void Invariant<M,N>::flog_once( size_t mode )
{
    //! Every 8th access is a write access
    bool write = (mode%8)==7;
    bool okay = true;
    bool lock_kept = true;
    if( (mode/8)&1 ) {
        // Try implicit acquire and explicit release
        typename mutex_type::scoped_lock lock(mutex,write);
        if( write ) {
            long my_value = value[0];
            update();
            if( mode%16==7 ) {
                lock_kept = lock.downgrade_to_reader();
                if( !lock_kept )
                    my_value = value[0] - 1;
                okay = value_is(my_value+1);
            }
        } else {
            okay = is_okay();
            if( mode%8==3 ) {
                long my_value = value[0];
                lock_kept = lock.upgrade_to_writer();
                if( !lock_kept )
                    my_value = value[0];
                update();
                okay = value_is(my_value+1);
            }
        }
        lock.release();
    } else {
        // Try explicit acquire and implicit release
        typename mutex_type::scoped_lock lock;
        lock.acquire(mutex,write);
        if( write ) {
            long my_value = value[0];
            update();
            if( mode%16==7 ) {
                lock_kept = lock.downgrade_to_reader();
                if( !lock_kept )
                    my_value = value[0] - 1;
                okay = value_is(my_value+1);
            }
        } else {
            okay = is_okay();
            if( mode%8==3 ) {
                long my_value = value[0];
                lock_kept = lock.upgrade_to_writer();
                if( !lock_kept )
                    my_value = value[0];
                update();
                okay = value_is(my_value+1);
            }
        }
    }
    if( !okay ) {
        REPORT( "ERROR for %s at %ld: %s %s %s %s\n",mutex_name, long(mode),
                write?"write,":"read,", write?(mode%16==7?"downgrade,":""):(mode%8==3?"upgrade,":""),
                lock_kept?"lock kept,":"lock not kept,", (mode/8)&1?"imp/exp":"exp/imp" );
    }
}

static tbb::atomic<size_t> Order;

template<typename State, long TestSize>
struct Work: NoAssign {
    static const size_t chunk = 100;
    State& state;
    Work( State& state_ ) : state(state_) {}
    void operator()( int ) const {
        size_t step;
        while( (step=Order.fetch_and_add<tbb::acquire>(chunk))<TestSize )
            for( size_t i=0; i<chunk && step<TestSize; ++i, ++step ) 
                state.flog_once(step);
    }
};

//! Generic test of a TBB Mutex type M.
/** Does not test features specific to reader-writer locks. */
template<typename M>
void Test( const char * name, int nthread ) {
    REMARK("testing %s\n",name);
    Counter<M> counter;
    counter.value = 0;
    Order = 0;
    // use the macro because of a gcc 4.6 bug
#define TEST_SIZE 100000
    tbb::tick_count t0 = tbb::tick_count::now();
    NativeParallelFor( nthread, Work<Counter<M>, TEST_SIZE>(counter) );
    tbb::tick_count t1 = tbb::tick_count::now();

    REMARK("%s time = %g usec\n",name, (t1-t0).seconds() );
    if( counter.value!=TEST_SIZE )
        REPORT("ERROR for %s: counter.value=%ld != %ld=test_size\n",name,counter.value,TEST_SIZE);
#undef TEST_SIZE
}


//! Generic test of TBB ReaderWriterMutex type M
template<typename M>
void TestReaderWriter( const char * mutex_name, int nthread ) {
    REMARK("testing %s\n",mutex_name);
    Invariant<M,8> invariant(mutex_name);
    Order = 0;
    // use the macro because of a gcc 4.6 bug
#define TEST_SIZE 1000000
    tbb::tick_count t0 = tbb::tick_count::now();
    NativeParallelFor( nthread, Work<Invariant<M,8>, TEST_SIZE>(invariant) );
    tbb::tick_count t1 = tbb::tick_count::now();
    // There is either a writer or a reader upgraded to a writer for each 4th iteration
    long expected_value = TEST_SIZE/4;
    if( !invariant.value_is(expected_value) )
        REPORT("ERROR for %s: final invariant value is wrong\n",mutex_name);
    REMARK("%s readers & writers time = %g usec\n",mutex_name,(t1-t0).seconds());
#undef TEST_SIZE
}

int TestMain () {
    for( int p=MinThread; p<=MaxThread; ++p ) {
        REMARK( "testing with %d threads\n", p );
        Test<tbb::spin_mutex>( "spin_mutex", p );
        Test<tbb::queuing_mutex>( "queuing_mutex", p );
        Test<tbb::queuing_rw_mutex>( "queuing_rw_mutex", p );
        Test<tbb::spin_rw_mutex>( "spin_rw_mutex", p );
        TestReaderWriter<tbb::queuing_rw_mutex>( "queuing_rw_mutex", p );
        TestReaderWriter<tbb::spin_rw_mutex>( "spin_rw_mutex", p );
    }
    return Harness::Done;
}
