#include "sixtracklib/cuda/track_job.h"

#include <iomanip>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "sixtracklib/testlib.h"

#include "sixtracklib/common/definitions.h"
#include "sixtracklib/common/control/definitions.h"
#include "sixtracklib/common/track/definitions.h"
#include "sixtracklib/common/control/node_info.h"
#include "sixtracklib/common/buffer.h"
#include "sixtracklib/common/particles.h"
#include "sixtracklib/common/be_monitor/be_monitor.h"

TEST( C99_CudaTrackJobSetupBeamMonitorTests,
      TrackUntilBeamMonitorOutputBufferTest )
{
    namespace st = SIXTRL_CXX_NAMESPACE;

    using particles_t    = ::NS(Particles);
    using track_job_t    = ::NS(CudaTrackJob);
    using buffer_t       = ::NS(Buffer);
    using buf_size_t     = ::NS(buffer_size_t);
    using track_status_t = ::NS(track_status_t);
    using ctrl_status_t  = ::NS(arch_status_t);
    using be_monitor_t   = ::NS(BeamMonitor);
    using pindex_t       = ::NS(particle_index_t);

    static constexpr buf_size_t NUM_PARTICLES = buf_size_t{ 1000 };
    static constexpr buf_size_t NUM_TURNS     = buf_size_t{ 5000 };

    buffer_t* pb = ::NS(Buffer_new)( buf_size_t{ 0 } );
    SIXTRL_ASSERT( pb != nullptr );

    particles_t* particle_set = ::NS(Particles_new)( pb, NUM_PARTICLES );
    SIXTRL_ASSERT( particle_set != nullptr );

    buffer_t* beam_elem_buffer = ::NS(Buffer_new_from_file)(
        ::NS(PATH_TO_BEAMBEAM_BEAM_ELEMENTS) );
    SIXTRL_ASSERT( beam_elem_buffer != nullptr );

    ::NS(Particles_realistic_init)( particle_set );

    pindex_t min_particle_id = pindex_t{ 0 };
    pindex_t max_particle_id = pindex_t{ 0 };

    ctrl_status_t status = ::NS(Particles_get_min_max_particle_id)(
        particle_set, &min_particle_id, &max_particle_id );

    SIXTRL_ASSERT( status == ::NS(ARCH_STATUS_SUCCESS) );
    SIXTRL_ASSERT( min_particle_id == pindex_t{ 0 } );
    SIXTRL_ASSERT( max_particle_id ==
        static_cast< pindex_t >( NUM_PARTICLES ) - 1 );

    buf_size_t be_mon_idx = ::NS(Buffer_get_num_of_objects)( beam_elem_buffer );
    ( void )be_mon_idx;

    be_monitor_t* be_monitor = ::NS(BeamMonitor_new)( beam_elem_buffer );
    SIXTRL_ASSERT( be_monitor != nullptr );

    ::NS(BeamMonitor_set_num_stores)( be_monitor, NUM_TURNS );

    track_job_t* track_job = ::NS(CudaTrackJob_new)(
        "0.0", pb, beam_elem_buffer );
    ASSERT_TRUE( track_job != nullptr );

    ASSERT_TRUE( ::NS(TrackJobNew_get_num_beam_monitors)(
        track_job ) == buf_size_t{ 1 } );

    ASSERT_TRUE( ::NS(TrackJobNew_get_num_particle_sets)(
        track_job ) == buf_size_t{ 1 } );

    ASSERT_TRUE( ::NS(TrackJobNew_get_total_num_of_particles)(
        track_job ) == NUM_PARTICLES );

    ASSERT_TRUE( ::NS(TrackJobNew_get_min_particle_id)(
        track_job ) == min_particle_id );

    ASSERT_TRUE( ::NS(TrackJobNew_get_max_particle_id)(
        track_job ) == max_particle_id );

    ASSERT_TRUE( ::NS(TrackJobNew_has_output_buffer)( track_job ) );
    ASSERT_TRUE( ::NS(TrackJobNew_owns_output_buffer)( track_job ) );

    /* Set to silly values so we can verify that the collect call actually did
     * work! */

    ::NS(BeamMonitor_set_min_particle_id)( be_monitor, -1 );
    ::NS(BeamMonitor_set_max_particle_id)( be_monitor, -1 );

    status = ::NS(TrackJobNew_collect_beam_elements)( track_job );
    ASSERT_TRUE( status == ::NS(ARCH_STATUS_SUCCESS) );

    SIXTRL_ASSERT( ::NS(BeamMonitor_get_min_particle_id)( be_monitor ) ==
                   min_particle_id );

    SIXTRL_ASSERT( ::NS(BeamMonitor_get_max_particle_id)( be_monitor ) ==
                   max_particle_id );

    ::NS(TrackJobNew_delete)( track_job );
    track_job = nullptr;

    /* --------------------------------------------------------------------- */

    ::NS(BeamMonitor_set_min_particle_id)( be_monitor, -1 );
    ::NS(BeamMonitor_set_max_particle_id)( be_monitor, -1 );

    buf_size_t num_objects   = buf_size_t{ 0 };
    buf_size_t num_slots     = buf_size_t{ 0 };
    buf_size_t num_data_ptrs = buf_size_t{ 0 };
    buf_size_t num_garbage   = buf_size_t{ 0 };

    status = ::NS(OutputBuffer_calculate_output_buffer_params)(
        beam_elem_buffer, particle_set, buf_size_t{ 0 }, &num_objects,
            &num_slots, &num_data_ptrs, &num_garbage,
                ::NS(Buffer_get_slot_size)( pb ) );

    SIXTRL_ASSERT( status == NS(ARCH_STATUS_SUCCESS) );
    SIXTRL_ASSERT( num_objects == buf_size_t{ 1 } );
    SIXTRL_ASSERT( num_slots > buf_size_t{ 0 } );
    SIXTRL_ASSERT( num_data_ptrs ==
        ::NS(Particles_get_required_num_dataptrs)( pb, NUM_PARTICLES ) );

    buffer_t* output_buffer = ::NS(Buffer_new_detailed)(
        num_objects, num_slots, num_data_ptrs, num_garbage,
            ::NS(BUFFER_DATASTORE_MEMPOOL) );

    SIXTRL_ASSERT( output_buffer != nullptr );

    track_job = ::NS(CudaTrackJob_new_with_output)(
        "0.0", pb, beam_elem_buffer, output_buffer, buf_size_t{ 0 } );
    ASSERT_TRUE( track_job != nullptr );

    ASSERT_TRUE( ::NS(TrackJobNew_get_num_beam_monitors)(
        track_job ) == buf_size_t{ 1 } );

    ASSERT_TRUE( ::NS(TrackJobNew_get_num_particle_sets)(
        track_job ) == buf_size_t{ 1 } );

    ASSERT_TRUE( ::NS(TrackJobNew_get_total_num_of_particles)(
        track_job ) == NUM_PARTICLES );

    ASSERT_TRUE( ::NS(TrackJobNew_get_min_particle_id)(
        track_job ) == min_particle_id );

    ASSERT_TRUE( ::NS(TrackJobNew_get_max_particle_id)(
        track_job ) == max_particle_id );

    ASSERT_TRUE(  ::NS(TrackJobNew_has_output_buffer)( track_job ) );
    ASSERT_TRUE( !::NS(TrackJobNew_owns_output_buffer)( track_job ) );

    ::NS(BeamMonitor_set_min_particle_id)( be_monitor, -1 );
    ::NS(BeamMonitor_set_max_particle_id)( be_monitor, -1 );

    status = ::NS(TrackJobNew_collect_beam_elements)( track_job );
    ASSERT_TRUE( status == ::NS(ARCH_STATUS_SUCCESS) );

    SIXTRL_ASSERT( ::NS(BeamMonitor_get_min_particle_id)( be_monitor ) ==
                   min_particle_id );

    SIXTRL_ASSERT( ::NS(BeamMonitor_get_max_particle_id)( be_monitor ) ==
                   max_particle_id );

    ::NS(TrackJobNew_delete)( track_job );
    ::NS(Buffer_delete)( output_buffer );
    track_job = nullptr;
    output_buffer = nullptr;

    /* --------------------------------------------------------------------- */

    size_t const pset_begin = size_t{ 0 };

    track_job = ::NS(CudaTrackJob_create)();
    SIXTRL_ASSERT( track_job != nullptr );

    output_buffer = ::NS(Buffer_new_detailed)( num_objects, num_slots,
        num_data_ptrs, num_garbage, ::NS(BUFFER_DATASTORE_MEMPOOL) );
    SIXTRL_ASSERT( output_buffer != nullptr );

    status = ::NS(TrackJobNew_reset_detailed)( track_job, pb, size_t{ 1 },
                &pset_begin, beam_elem_buffer, output_buffer, size_t{ 0 } );

    SIXTRL_ASSERT( status == st::ARCH_STATUS_SUCCESS );

    ASSERT_TRUE( ::NS(TrackJobNew_get_num_beam_monitors)(
        track_job ) == buf_size_t{ 1 } );

    ASSERT_TRUE( ::NS(TrackJobNew_get_num_particle_sets)(
        track_job ) == buf_size_t{ 1 } );

    ASSERT_TRUE( ::NS(TrackJobNew_get_total_num_of_particles)(
        track_job ) == NUM_PARTICLES );

    ASSERT_TRUE( ::NS(TrackJobNew_get_min_particle_id)(
        track_job ) == min_particle_id );

    ASSERT_TRUE( ::NS(TrackJobNew_get_max_particle_id)(
        track_job ) == max_particle_id );

    ASSERT_TRUE(  ::NS(TrackJobNew_has_output_buffer)( track_job ) );
    ASSERT_TRUE( !::NS(TrackJobNew_owns_output_buffer)( track_job ) );

    ::NS(BeamMonitor_set_min_particle_id)( be_monitor, -1 );
    ::NS(BeamMonitor_set_max_particle_id)( be_monitor, -1 );

    status = ::NS(TrackJobNew_collect_beam_elements)( track_job );
    ASSERT_TRUE( status == ::NS(ARCH_STATUS_SUCCESS) );

    SIXTRL_ASSERT( ::NS(BeamMonitor_get_min_particle_id)( be_monitor ) ==
                   min_particle_id );

    SIXTRL_ASSERT( ::NS(BeamMonitor_get_max_particle_id)( be_monitor ) ==
                   max_particle_id );

    /* --------------------------------------------------------------------- */

    ::NS(TrackJobNew_delete)( track_job );

    ::NS(Buffer_delete)( pb );
    ::NS(Buffer_delete)( output_buffer );
    ::NS(Buffer_delete)( beam_elem_buffer );
}

/* end: tests/sixtracklib/cuda/track/test_track_job_setup_beam_monitor_c99.cpp */
