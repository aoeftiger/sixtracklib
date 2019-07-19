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
#include "sixtracklib/common/control/node_info.hpp"
#include "sixtracklib/common/buffer.hpp"
#include "sixtracklib/common/particles.hpp"
#include "sixtracklib/common/be_monitor/be_monitor.hpp"

TEST( CXX_CudaTrackJobSetupBeamMonitorTests,
      TrackUntilBeamMonitorOutputBufferTest )
{
    namespace st = SIXTRL_CXX_NAMESPACE;

    using particles_t    = st::Particles;
    using track_job_t    = st::CudaTrackJob;
    using be_monitor_t   = st::BeamMonitor;
    using buffer_t       = st::Buffer;
    using status_t       = st::arch_status_t;
    using buf_size_t     = buffer_t::size_type;
    using track_status_t = track_job_t::track_status_t;
    using pindex_t       = particles_t::index_t;

    static constexpr buf_size_t NUM_PARTICLES = buf_size_t{ 1000 };
    static constexpr buf_size_t NUM_TURNS     = buf_size_t{ 5000 };

    buffer_t pb;
    particles_t* particle_set = pb.createNew< particles_t >( NUM_PARTICLES );
    SIXTRL_ASSERT( particle_set != nullptr );

    buffer_t beam_elem_buffer( ::NS(PATH_TO_BEAMBEAM_BEAM_ELEMENTS) );

    ::NS(Particles_realistic_init)( particle_set->getCApiPtr() );

    pindex_t min_particle_id = pindex_t{ 0 };
    pindex_t max_particle_id = pindex_t{ 0 };

    status_t status = ::NS(Particles_get_min_max_particle_id)(
        particle_set->getCApiPtr(), &min_particle_id, &max_particle_id );

    SIXTRL_ASSERT( status == st::ARCH_STATUS_SUCCESS );
    SIXTRL_ASSERT( min_particle_id == pindex_t{ 0 } );
    SIXTRL_ASSERT( max_particle_id ==
        static_cast< pindex_t >( NUM_PARTICLES ) - 1 );

    buf_size_t be_mon_idx = beam_elem_buffer.getNumObjects();
    ( void )be_mon_idx;

    be_monitor_t* be_monitor = beam_elem_buffer.createNew< be_monitor_t >();
    SIXTRL_ASSERT( be_monitor != nullptr );

    be_monitor->setNumStores( NUM_TURNS );

    std::unique_ptr< track_job_t > track_job(
        new track_job_t("0.0", pb, beam_elem_buffer ) );

    ASSERT_TRUE( track_job.get() != nullptr );

    ASSERT_TRUE( track_job->numBeamMonitors()   == buf_size_t{ 1 } );
    ASSERT_TRUE( track_job->numParticleSets()   == buf_size_t{ 1 } );
    ASSERT_TRUE( track_job->totalNumParticles() == NUM_PARTICLES );

    ASSERT_TRUE( track_job->minParticleId() == min_particle_id );
    ASSERT_TRUE( track_job->maxParticleId() == max_particle_id );
    ASSERT_TRUE( track_job->hasOutputBuffer() );
    ASSERT_TRUE( track_job->ownsOutputBuffer() );

    /* Set to silly values so we can verify that the collect call actually did
     * work! */

    be_monitor->setMinParticleId( -1 );
    be_monitor->setMaxParticleId( -1 );

    status = track_job->collectBeamElements();

    SIXTRL_ASSERT( be_monitor->getMinParticleId() == min_particle_id );
    SIXTRL_ASSERT( be_monitor->getMaxParticleId() == max_particle_id );

    track_job.reset( nullptr );

    /* --------------------------------------------------------------------- */
    /* Init the track job with an external output buffer in the first place */

    be_monitor->setMinParticleId( -1 );
    be_monitor->setMaxParticleId( -1 );

    buf_size_t num_objects   = buf_size_t{ 0 };
    buf_size_t num_slots     = buf_size_t{ 0 };
    buf_size_t num_data_ptrs = buf_size_t{ 0 };
    buf_size_t num_garbage   = buf_size_t{ 0 };

    status = ::NS(OutputBuffer_calculate_output_buffer_params)(
        beam_elem_buffer.getCApiPtr(), particle_set, buf_size_t{ 0 },
           &num_objects, &num_slots, &num_data_ptrs, &num_garbage,
               pb.getSlotSize() );

    SIXTRL_ASSERT( status == st::ARCH_STATUS_SUCCESS );
    SIXTRL_ASSERT( num_objects == buf_size_t{ 1 } );
    SIXTRL_ASSERT( num_slots > buf_size_t{ 0 } );
    SIXTRL_ASSERT( num_data_ptrs ==
        particles_t::RequiredNumDataPtrs( pb, NUM_PARTICLES ) );

    std::unique_ptr< buffer_t > output_buffer( new buffer_t(
        num_objects, num_slots, num_data_ptrs, num_garbage ) );

    ASSERT_TRUE( output_buffer.get() != nullptr );

    track_job.reset( new track_job_t(
        "0.0", pb, beam_elem_buffer, output_buffer.get() ) );

    ASSERT_TRUE(  track_job.get() != nullptr );
    ASSERT_TRUE(  track_job->numBeamMonitors()   == buf_size_t{ 1 } );
    ASSERT_TRUE(  track_job->numParticleSets()   == buf_size_t{ 1 } );
    ASSERT_TRUE(  track_job->totalNumParticles() == NUM_PARTICLES );

    ASSERT_TRUE(  track_job->minParticleId() == min_particle_id );
    ASSERT_TRUE(  track_job->maxParticleId() == max_particle_id );
    ASSERT_TRUE(  track_job->hasOutputBuffer() );
    ASSERT_TRUE( !track_job->ownsOutputBuffer() );

    /* Set to silly values so we can verify that the collect call actually did
     * work! */

    be_monitor->setMinParticleId( -1 );
    be_monitor->setMaxParticleId( -1 );

    status = track_job->collectBeamElements();

    SIXTRL_ASSERT( be_monitor->getMinParticleId() == min_particle_id );
    SIXTRL_ASSERT( be_monitor->getMaxParticleId() == max_particle_id );

    track_job.reset( nullptr );
    output_buffer.reset( nullptr );

    /* --------------------------------------------------------------------- */
    /* Reset default initialized track job to use the beam monitors */

    track_job.reset( new track_job_t );
    ASSERT_TRUE(  track_job.get() != nullptr );

    output_buffer.reset( new buffer_t( num_objects, num_slots,
        num_data_ptrs, num_garbage ) );

    ASSERT_TRUE( output_buffer.get() != nullptr );

    buf_size_t const pset_begin{ 0 };

    status = track_job->reset( pb.getCApiPtr(), buf_size_t{ 1 },
                &pset_begin, beam_elem_buffer.getCApiPtr(),
                output_buffer->getCApiPtr() );

    ASSERT_TRUE( status == st::ARCH_STATUS_SUCCESS );

    ASSERT_TRUE(  track_job.get() != nullptr );
    ASSERT_TRUE(  track_job->numBeamMonitors()   == buf_size_t{ 1 } );
    ASSERT_TRUE(  track_job->numParticleSets()   == buf_size_t{ 1 } );
    ASSERT_TRUE(  track_job->totalNumParticles() == NUM_PARTICLES );

    ASSERT_TRUE(  track_job->minParticleId() == min_particle_id );
    ASSERT_TRUE(  track_job->maxParticleId() == max_particle_id );
    ASSERT_TRUE(  track_job->hasOutputBuffer() );
    ASSERT_TRUE( !track_job->ownsOutputBuffer() );

    /* Set to silly values so we can verify that the collect call actually did
     * work! */

    be_monitor->setMinParticleId( -1 );
    be_monitor->setMaxParticleId( -1 );

    status = track_job->collectBeamElements();

    SIXTRL_ASSERT( be_monitor->getMinParticleId() == min_particle_id );
    SIXTRL_ASSERT( be_monitor->getMaxParticleId() == max_particle_id );
}

/* end: tests/sixtracklib/cuda/track/test_track_job_setup_beam_monitor_cxx.cpp */
