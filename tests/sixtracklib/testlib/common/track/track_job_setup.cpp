#include "sixtracklib/testlib/common/track/track_job_setup.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "sixtracklib/common/definitions.h"
#include "sixtracklib/common/particles.hpp"
#include "sixtracklib/common/be_monitor/be_monitor.hpp"
#include "sixtracklib/common/output/output_buffer.h"
#include "sixtracklib/common/track/track_job_base.hpp"
#include "sixtracklib/common/track/track_job_ctrl_arg_base.hpp"
#include "sixtracklib/common/track/track_job_nodectrl_arg_base.hpp"

namespace st = SIXTRL_CXX_NAMESPACE;

namespace SIXTRL_CXX_NAMESPACE
{
    namespace tests
    {
        bool TestTrackJob_setup_no_required_output(
            st::TrackJobBaseNew const& SIXTRL_RESTRICT_REF job,
            st::Buffer const& SIXTRL_RESTRICT_REF particles_buffer,
            st::Buffer::size_type const num_psets,
            st::Buffer::size_type const* SIXTRL_RESTRICT pset_indices_begin,
            st::Buffer const& SIXTRL_RESTRICT_REF beam_elements_buffer,
            st::Buffer const* SIXTRL_RESTRICT ptr_output_buffer )
        {
            using buffer_t = st::Buffer;
            using size_t   = buffer_t::size_type;

            bool success = (
                ( ( ( ptr_output_buffer == nullptr ) &&
                    ( !job.hasOutputBuffer() ) && ( !job.ownsOutputBuffer() ) ) ||
                  ( ( ptr_output_buffer != nullptr ) &&
                    (  job.hasOutputBuffer() ) &&
                    ( !job.ownsOutputBuffer() ) ) ) &&
                ( !job.hasBeamMonitorOutput() ) &&
                (  job.numBeamMonitors() == size_t{ 0 } ) );

            if( success )
            {
                success = ( ( job.beamMonitorIndicesBegin() == nullptr ) &&
                            ( job.beamMonitorIndicesEnd()   == nullptr ) );
            }

            if( success )
            {
                success = ( (  job.numParticleSets() == num_psets ) &&
                    (  job.particleSetIndicesBegin() != nullptr ) &&
                    (  job.particleSetIndicesEnd()   != nullptr ) &&+
                    ( std::equal( job.particleSetIndicesBegin(),
                                  job.particleSetIndicesEnd(),
                                  pset_indices_begin ) ) );
            }

            if( success )
            {
                success = ( ( !job.hasElemByElemOutput() ) &&
                    ( job.ptrElemByElemConfig() == nullptr ) );
            }

            if( success )
            {
                if( ptr_output_buffer != nullptr )
                {
                    success = ( ( job.ptrOutputBuffer() == ptr_output_buffer ) &&
                        ( job.hasOutputBuffer() ) &&
                        ( !job.ownsOutputBuffer() ) );
                }
                else
                {
                    success = ( ( job.ptrOutputBuffer() == nullptr ) &&
                        ( !job.hasOutputBuffer() ) &&
                        ( !job.ownsOutputBuffer() ) );
                }
            }

            if( success )
            {
                success = (
                    ( job.ptrParticlesBuffer() == &particles_buffer ) &&
                    ( job.ptrBeamElementsBuffer() == &beam_elements_buffer ) );
            }

            return success;
        }


        bool TestTrackJob_setup_no_beam_monitors_elem_by_elem(
            st::TrackJobBaseNew const& SIXTRL_RESTRICT_REF job,
            st::Buffer const& SIXTRL_RESTRICT_REF particles_buffer,
            st::Buffer::size_type const num_psets,
            st::Buffer::size_type const* SIXTRL_RESTRICT pset_indices_begin,
            st::Buffer const& SIXTRL_RESTRICT_REF beam_elements_buffer,
            st::Buffer const* SIXTRL_RESTRICT ptr_output_buffer,
            st::Buffer::size_type const until_turn_elem_by_elem )
        {
            bool success = false;

            using track_job_t         = st::TrackJobBaseNew;
            using buffer_t            = track_job_t::buffer_t;
            using size_t              = track_job_t::size_type;
            using elem_by_elem_conf_t = track_job_t::elem_by_elem_config_t;
            using status_t            = st::ctrl_status_t;
            using particles_t         = st::Particles;
            using pindex_t            = particles_t::index_t;

            buffer_t const* output_buffer = nullptr;
            elem_by_elem_conf_t const* elem_by_elem_conf = nullptr;

            SIXTRL_ASSERT( until_turn_elem_by_elem > size_t{ 0 } );

            particles_t const* particles = particles_t::FromBuffer(
                particles_buffer, size_t{ 0 } );

            SIXTRL_ASSERT( particles != nullptr );

            size_t const NUM_BEAM_ELEMENTS =
                beam_elements_buffer.getNumObjects();

            size_t const NUM_PARTICLES = particles->getNumParticles();

            success = (
                ( NUM_BEAM_ELEMENTS > size_t{ 0 } ) &&
                ( NUM_PARTICLES > size_t{ 0 } ) &&
                (  job.hasOutputBuffer() ) &&
                ( !job.hasBeamMonitorOutput() ) &&
                (  job.numBeamMonitors() == size_t{ 0 } ) &&
                (  job.beamMonitorIndicesBegin() == nullptr ) &&
                (  job.beamMonitorIndicesEnd()   == nullptr ) );

            if( success )
            {
                 success = ( (  job.numParticleSets() == num_psets ) &&
                    (  job.particleSetIndicesBegin() != nullptr ) &&
                    (  job.particleSetIndicesEnd()   != nullptr ) &&+
                    ( std::equal( job.particleSetIndicesBegin(),
                                  job.particleSetIndicesEnd(),
                                  pset_indices_begin ) ) );
            }

            if( success )
            {
                elem_by_elem_conf = job.ptrElemByElemConfig();

                success = ( ( elem_by_elem_conf != nullptr ) &&
                    ( ::NS(ElemByElemConfig_is_active)( elem_by_elem_conf ) ) &&
                    ( static_cast< size_t >(
                        ::NS(ElemByElemConfig_get_out_store_num_particles)(
                            elem_by_elem_conf ) ) >=
                        ( NUM_PARTICLES * NUM_BEAM_ELEMENTS
                            * until_turn_elem_by_elem ) ) &&
                    ( ::NS(ElemByElemConfig_is_rolling)( elem_by_elem_conf ) ==
                      job.defaultElemByElemRolling() ) );
            }

            if( success )
            {
                success = false;

                pindex_t min_particle_id   = pindex_t{ 0 };
                pindex_t max_particle_id   = pindex_t{ 0 };
                pindex_t min_at_element_id = pindex_t{ 0 };
                pindex_t max_at_element_id = pindex_t{ 0 };
                pindex_t min_at_turn_id    = pindex_t{ 0 };
                pindex_t max_at_turn_id    = pindex_t{ 0 };

                size_t num_elem_by_elem_objs = size_t{ 0 };
                pindex_t const start_elem_id = pindex_t{ 0 };

                status_t const status =
                    ::NS(OutputBuffer_get_min_max_attributes_on_particle_sets)(
                        particles_buffer.getCApiPtr(), num_psets,
                        pset_indices_begin, beam_elements_buffer.getCApiPtr(),
                        &min_particle_id, &max_particle_id, &min_at_element_id,
                        &max_at_element_id, &min_at_turn_id, &max_at_turn_id,
                        &num_elem_by_elem_objs, start_elem_id );

                if( ( status == st::ARCH_STATUS_SUCCESS ) &&
                    ( min_particle_id <= max_particle_id ) &&
                    ( min_at_element_id <= max_at_element_id ) &&
                    ( min_at_turn_id <= max_at_turn_id ) &&
                    ( num_elem_by_elem_objs > size_t{ 0 } ) )
                {
                    elem_by_elem_conf = job.ptrElemByElemConfig();
                    SIXTRL_ASSERT( elem_by_elem_conf != nullptr );

                    success = (
                        ( ::NS(ElemByElemConfig_get_min_particle_id)(
                            elem_by_elem_conf ) == min_particle_id ) &&
                        ( ::NS(ElemByElemConfig_get_max_particle_id)(
                            elem_by_elem_conf ) == max_particle_id ) &&
                        ( ::NS(ElemByElemConfig_get_min_element_id)(
                            elem_by_elem_conf ) == min_at_element_id ) &&
                        ( ::NS(ElemByElemConfig_get_max_element_id)(
                            elem_by_elem_conf ) == max_at_element_id ) &&
                        ( ::NS(ElemByElemConfig_get_min_turn)(
                            elem_by_elem_conf ) == min_at_turn_id ) &&
                        ( ::NS(ElemByElemConfig_get_max_turn)(
                            elem_by_elem_conf ) >= max_at_turn_id ) );
                }
            }

            if( success )
            {
                output_buffer = job.ptrOutputBuffer();

                success = ( ( output_buffer != nullptr ) &&
                    ( output_buffer->getNumObjects() == size_t{ 1 } ) &&
                    ( st::Buffer_is_particles_buffer( *output_buffer ) ) &&
                    ( st::Particles::FromBuffer(
                        *output_buffer, size_t{ 0 } )->getNumParticles()  >=
                      ::NS(ElemByElemConfig_get_out_store_num_particles)(
                        elem_by_elem_conf ) ) &&
                    (  job.beamMonitorsOutputBufferOffset() == size_t{ 1 } ) &&
                    (  job.beamMonitorsOutputBufferOffset() >=
                       output_buffer->getNumObjects() ) );
            }

            if( success )
            {
                if( ptr_output_buffer != nullptr )
                {
                    success = ( ( !job.ownsOutputBuffer() ) &&
                        ( job.ptrOutputBuffer() == ptr_output_buffer ) &&
                        ( job.hasOutputBuffer() ) );
                }
                else
                {
                    success = ( ( job.ownsOutputBuffer() ) &&
                        ( job.ptrOutputBuffer() != nullptr ) &&
                        ( job.hasOutputBuffer() ) );
                }
            }

            if( success )
            {
                success = (
                    ( job.ptrParticlesBuffer() == &particles_buffer ) &&
                    ( job.ptrBeamElementsBuffer() == &beam_elements_buffer ) );
            }

            return success;
        }


        bool TestTrackJob_setup_beam_monitors_and_elem_by_elem(
            st::TrackJobBaseNew const& SIXTRL_RESTRICT_REF job,
            st::Buffer const& SIXTRL_RESTRICT_REF particles_buffer,
            st::Buffer::size_type const num_psets,
            st::Buffer::size_type const* SIXTRL_RESTRICT pset_indices_begin,
            st::Buffer const& SIXTRL_RESTRICT_REF beam_elements_buffer,
            st::Buffer const* SIXTRL_RESTRICT ext_output_buffer,
            st::Buffer::size_type const num_beam_monitors,
            st::Buffer::size_type const until_turn,
            st::Buffer::size_type const until_turn_elem_by_elem )
        {
            bool success = false;

            using track_job_t         = st::TrackJobBaseNew;
            using buffer_t            = track_job_t::buffer_t;
            using size_t              = track_job_t::size_type;
            using elem_by_elem_conf_t = track_job_t::elem_by_elem_config_t;
            using status_t            = st::ctrl_status_t;
            using particles_t         = st::Particles;
            using pindex_t            = particles_t::index_t;
            using be_monitor_t        = st::BeamMonitor;

            pindex_t min_particle_id   = pindex_t{ 0 };
            pindex_t max_particle_id   = pindex_t{ 0 };
            pindex_t min_at_element_id = pindex_t{ 0 };
            pindex_t max_at_element_id = pindex_t{ 0 };
            pindex_t min_at_turn_id    = pindex_t{ 0 };
            pindex_t max_at_turn_id    = pindex_t{ 0 };

            size_t num_elem_by_elem_objs = size_t{ 0 };
            pindex_t const start_elem_id = pindex_t{ 0 };

            status_t status =
                ::NS(OutputBuffer_get_min_max_attributes_on_particle_sets)(
                    particles_buffer.getCApiPtr(), num_psets,
                    pset_indices_begin, beam_elements_buffer.getCApiPtr(),
                    &min_particle_id, &max_particle_id, &min_at_element_id,
                    &max_at_element_id, &min_at_turn_id, &max_at_turn_id,
                    &num_elem_by_elem_objs, start_elem_id );

            if( ( status != st::ARCH_STATUS_SUCCESS ) ||
                ( min_particle_id > max_particle_id ) ||
                ( min_at_element_id > max_at_element_id ) ||
                ( min_at_turn_id > max_at_turn_id ) ||
                ( num_elem_by_elem_objs == size_t{ 0 } ) )
            {
                return false;
            }

            buffer_t const* output_buffer = nullptr;
            elem_by_elem_conf_t const* elem_by_elem_conf = nullptr;

            particles_t const* particles = particles_t::FromBuffer(
                particles_buffer, size_t{ 0 } );

            SIXTRL_ASSERT( particles != nullptr );

            size_t const NUM_BEAM_ELEMENTS = beam_elements_buffer.getNumObjects();
            size_t const NUM_PARTICLES = particles->getNumParticles();

            success = (
                ( NUM_BEAM_ELEMENTS > size_t{ 0 } ) &&
                ( NUM_PARTICLES > size_t{ 0 } ) &&
                ( job.numBeamMonitors() == num_beam_monitors ) );

            if( success )
            {
                 success = ( (  job.numParticleSets() == num_psets ) &&
                    (  job.particleSetIndicesBegin() != nullptr ) &&
                    (  job.particleSetIndicesEnd()   != nullptr ) &&+
                    ( std::equal( job.particleSetIndicesBegin(),
                                  job.particleSetIndicesEnd(),
                                  pset_indices_begin ) ) );
            }

            if( ( success ) && ( num_beam_monitors > size_t{ 0 } ) )
            {
                size_t const* be_mon_idx_it  = job.beamMonitorIndicesBegin();
                size_t const* be_mon_idx_end = job.beamMonitorIndicesEnd();

                success = (
                    ( be_mon_idx_it  != nullptr ) &&
                    ( be_mon_idx_end != nullptr ) &&
                    ( job.beamMonitorsOutputBufferOffset() >=
                      job.elemByElemOutputBufferOffset() ) &&
                    ( std::ptrdiff_t{ 0 } < std::distance(
                        be_mon_idx_it, be_mon_idx_end ) ) &&
                    ( static_cast< size_t >( std::distance(
                        be_mon_idx_it, be_mon_idx_end ) ) == num_beam_monitors ) );

                if( success )
                {
                    for( ; be_mon_idx_it != be_mon_idx_end ; ++be_mon_idx_it )
                    {
                        be_monitor_t const* mon = beam_elements_buffer.get<
                            be_monitor_t >( *be_mon_idx_it );

                        if( mon == nullptr )
                        {
                            success = false;
                            break;
                        }

                        if( ( mon->getMinParticleId() != min_particle_id ) ||
                            ( mon->getMaxParticleId() != max_particle_id ) )
                        {
                            success = false;
                            break;
                        }
                    }
                }
            }

            if( ( success ) && ( until_turn_elem_by_elem > size_t{ 0 } ) )
            {
                elem_by_elem_conf = job.ptrElemByElemConfig();

                success = (
                    ( job.hasElemByElemOutput() ) &&
                    ( job.ptrElemByElemConfig() != nullptr ) &&
                    ( job.elemByElemOutputBufferOffset() == size_t{ 0 } ) &&
                    ( elem_by_elem_conf != nullptr ) &&
                    ( ::NS(ElemByElemConfig_is_active)( elem_by_elem_conf ) ) &&
                    ( static_cast< size_t >(
                        ::NS(ElemByElemConfig_get_out_store_num_particles)(
                            elem_by_elem_conf ) ) >=
                        ( NUM_PARTICLES * NUM_BEAM_ELEMENTS
                            * until_turn_elem_by_elem ) ) &&
                    ( ::NS(ElemByElemConfig_is_rolling)( elem_by_elem_conf ) ==
                       job.defaultElemByElemRolling() ) );
            }

            if( ( success ) && ( job.hasElemByElemOutput() ) )
            {
                success = false;

                auto conf = job.ptrElemByElemConfig();
                SIXTRL_ASSERT( elem_by_elem_conf != nullptr );

                success = (
                    ( ::NS(ElemByElemConfig_get_min_particle_id)( conf ) ==
                        min_particle_id ) &&
                    ( ::NS(ElemByElemConfig_get_max_particle_id)( conf ) ==
                        max_particle_id ) &&
                    ( ::NS(ElemByElemConfig_get_min_element_id)( conf ) ==
                        min_at_element_id ) &&
                    ( ::NS(ElemByElemConfig_get_max_element_id)( conf ) ==
                        max_at_element_id ) &&
                    ( ::NS(ElemByElemConfig_get_min_turn)( conf ) ==
                        min_at_turn_id ) &&
                    ( ::NS(ElemByElemConfig_get_max_turn)( conf ) >=
                        max_at_turn_id ) );
            }

            if( ( success ) &&
                ( ( until_turn_elem_by_elem > size_t{ 0 } ) ||
                  ( until_turn > size_t{ 0 } ) ||
                  ( num_beam_monitors > size_t{ 0 } ) ) )
            {
                output_buffer = job.ptrOutputBuffer();

                size_t requ_num_output_elems = num_beam_monitors;

                if( job.hasElemByElemOutput() )
                {
                    requ_num_output_elems += size_t{ 1 };
                }

                success = (
                    ( output_buffer != nullptr ) &&
                    ( output_buffer->getNumObjects() == requ_num_output_elems ) &&
                    ( st::Buffer_is_particles_buffer( *output_buffer ) ) );
            }

            if( ( success ) && ( until_turn_elem_by_elem > size_t{ 0 } ) &&
                ( elem_by_elem_conf != nullptr ) )
            {
                success = ( output_buffer->get< st::Particles >(
                    size_t{ 0 } )->getNumParticles() >=
                    ::NS(ElemByElemConfig_get_out_store_num_particles)(
                        elem_by_elem_conf ) );
            }


            if( ( success ) && ( until_turn > size_t{ 0 } ) &&
                ( num_beam_monitors > size_t{ 0 } ) )
            {
                success = (
                    ( job.beamMonitorsOutputBufferOffset() >=
                      job.elemByElemOutputBufferOffset() ) &&
                    ( job.beamMonitorsOutputBufferOffset() <
                      output_buffer->getNumObjects() ) );
            }

            if( success )
            {
                if( ext_output_buffer != nullptr )
                {
                    success = (
                        ( !job.ownsOutputBuffer() ) &&
                        (  job.ptrOutputBuffer() == ext_output_buffer ) &&
                        (  job.hasOutputBuffer() ) );
                }
                else
                {
                    success = (
                        ( job.ownsOutputBuffer() ) &&
                        ( job.ptrOutputBuffer() != nullptr ) &&
                        ( job.hasOutputBuffer() ) );
                }
            }

            if( success )
            {
                success = (
                    ( job.ptrParticlesBuffer() == &particles_buffer ) &&
                    ( job.ptrBeamElementsBuffer() == &beam_elements_buffer ) );
            }

            return success;
        }

    }
}

/* end: tests/sixtracklib/testlib/common/track/track_job_setup.cpp */
