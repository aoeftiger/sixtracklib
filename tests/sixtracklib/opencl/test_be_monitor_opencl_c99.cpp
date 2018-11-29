#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "sixtracklib/common/definitions.h"
#include "sixtracklib/common/generated/path.h"
#include "sixtracklib/common/buffer.h"
#include "sixtracklib/common/be_drift/be_drift.h"
#include "sixtracklib/common/be_monitor/be_monitor.h"
#include "sixtracklib/common/be_monitor/output_buffer.h"
#include "sixtracklib/common/be_monitor/track.h"
#include "sixtracklib/common/track.h"
#include "sixtracklib/common/beam_elements.h"

#include "sixtracklib/opencl/context.h"
#include "sixtracklib/opencl/argument.h"
#include "sixtracklib/testlib/common/particles.h"

namespace sixtrack
{
    namespace tests
    {
        bool performBeamMonitorTrackingTest(
            ::st_ClContext* SIXTRL_RESTRICT context,
            ::st_Buffer* SIXTRL_RESTRICT beam_elements_buffer,
            ::st_Buffer const* SIXTRL_RESTRICT elem_by_elem_buffer,
            ::st_buffer_size_t const num_turns,
            double const abs_tolerance =
                std::numeric_limits< double >::epsilon() );

    }
}

TEST( C99_OpenCLBeamMonitorTests, AssignIoBufferToBeamMonitors )
{
    using real_t        = ::st_particle_real_t;
    using size_t        = ::st_buffer_size_t;
    using nturn_t       = ::st_be_monitor_turn_t;
    using addr_t        = ::st_be_monitor_addr_t;
    using type_id_t     = ::st_object_type_id_t;
    using part_index_t  = ::st_particle_index_t;
    using mon_index_t   = ::st_be_monitor_index_t;

    using turn_dist_t   = std::uniform_int_distribution< nturn_t >;
    using chance_dist_t = std::uniform_real_distribution< real_t >;

    std::mt19937_64::result_type const seed = 20181031u;

    std::mt19937_64 prng;
    prng.seed( seed );

    ::st_Buffer* eb = ::st_Buffer_new( 0u );

    size_t const NUM_BEAM_MONITORS  = size_t{  10 };
    size_t const NUM_DRIFTS         = size_t{ 100 };
    size_t const NUM_BEAM_ELEMENTS  = NUM_BEAM_MONITORS + NUM_DRIFTS;
    size_t const NUM_PARTICLES      = size_t{   2 };
    size_t const DRIFT_SEQU_LEN     = NUM_DRIFTS / NUM_BEAM_MONITORS;

    std::vector< ::st_BeamMonitor > cmp_beam_monitors;

    turn_dist_t num_stores_dist( 1, 100 );
    turn_dist_t start_dist( 0, 1000 );
    turn_dist_t skip_dist( 1, 100 );

    chance_dist_t rolling_dist( 0., 1. );
    size_t sum_num_of_stores = size_t{ 0 };

    for( size_t ii = size_t{ 0 } ; ii < NUM_BEAM_MONITORS ; ++ii )
    {
        for( size_t jj = size_t{ 0 } ; jj < DRIFT_SEQU_LEN ; ++jj )
        {
            ::st_Drift*  drift = ::st_Drift_add( eb, real_t{ 1.0 } );
            ASSERT_TRUE( drift != nullptr );
        }

        ::st_BeamMonitor* be_monitor = ::st_BeamMonitor_add( eb,
            num_stores_dist( prng ), start_dist( prng ), skip_dist( prng ),
            addr_t{ 0 }, part_index_t{ 0 }, part_index_t{ 0 },
                bool{ rolling_dist( prng ) >= 0.5 }, true );

        ASSERT_TRUE( be_monitor != nullptr );

        sum_num_of_stores += ::st_BeamMonitor_get_num_stores( be_monitor );
        cmp_beam_monitors.push_back( *be_monitor );
    }

    ASSERT_TRUE( ::st_BeamMonitor_get_num_elem_by_elem_objects( eb ) ==
                 NUM_BEAM_ELEMENTS );

    ASSERT_TRUE( ::st_BeamMonitor_get_num_of_beam_monitor_objects( eb ) ==
                 NUM_BEAM_MONITORS );

    /* --------------------------------------------------------------------- */
    /* get number of available OpenCL Nodes: */

    ::st_ClContext* context = ::st_ClContext_create();

    ASSERT_TRUE( context != nullptr );

    size_t const num_available_nodes =
        ::st_ClContextBase_get_num_available_nodes( context );

    ASSERT_TRUE( num_available_nodes > size_t{ 0 } );

    ::st_ClContext_delete( context );
    context = nullptr;

    for( size_t node_index = size_t{ 0 } ;
        node_index < num_available_nodes ; ++node_index )
    {
        context = ::st_ClContext_create();
        ::st_ClContextBase_enable_debug_mode( context );

        ASSERT_TRUE( ::st_ClContextBase_is_debug_mode_enabled( context ) );
        ASSERT_TRUE( ::st_ClContextBase_select_node_by_index(
            context, node_index ) );

        ASSERT_TRUE( ::st_ClContextBase_has_selected_node( context ) );

        ::st_context_node_info_t const* node_info =
            ::st_ClContextBase_get_selected_node_info( context );

        ASSERT_TRUE( node_info != nullptr );
        ASSERT_TRUE( ::st_ClContextBase_has_remapping_kernel( context ) );

        char id_str[ 32 ];
        ::st_ClContextBase_get_selected_node_id_str( context, id_str, 32 );

        std::cout << "# ------------------------------------------------------"
                  << "--------------------------------------------------------"
                  << "\r\n"
                  << "# Run Test on :: \r\n"
                  << "# ID          :: " << id_str << "\r\n"
                  << "# NAME        :: "
                  << ::st_ComputeNodeInfo_get_name( node_info ) << "\r\n"
                  << "# PLATFORM    :: "
                  << ::st_ComputeNodeInfo_get_platform( node_info ) << "\r\n"
                  << "# "
                  << std::endl;

        /* ----------------------------------------------------------------- */

        ::st_Buffer* pb = ::st_Buffer_new( 0u );
        ::st_Buffer* out_buffer = ::st_Buffer_new( 0u );

        part_index_t min_particle_id = std::numeric_limits< part_index_t >::max();
        part_index_t max_particle_id = std::numeric_limits< part_index_t >::min();

        ::st_Particles* particles = ::st_Particles_new( pb, NUM_PARTICLES );
        ::st_Particles_init_particle_ids( particles );

        ASSERT_TRUE( 0 == ::st_Particles_get_min_max_particle_id(
            particles, &min_particle_id, &max_particle_id ) );

        ASSERT_TRUE( min_particle_id >= part_index_t{ 0 } );
        ASSERT_TRUE( max_particle_id >= min_particle_id   );

        /* --------------------------------------------------------------------- */
        /* reserve out_buffer buffer without element by element buffer */

        ASSERT_TRUE( 0 == ::st_BeamMonitor_prepare_particles_out_buffer(
            eb, out_buffer, particles, 0u ) );

        ASSERT_TRUE( NUM_BEAM_MONITORS ==
            ::st_Particles_buffer_get_num_of_particle_blocks( out_buffer ) );

        ASSERT_TRUE( ( sum_num_of_stores * NUM_PARTICLES ) == static_cast< size_t >(
            ::st_Particles_buffer_get_total_num_of_particles( out_buffer ) ) );

        /* ----------------------------------------------------------------- */
        /* Check out_buffer addr to be 0 before sending it to the device */

        auto be_it  = ::st_Buffer_get_const_objects_begin( eb );
        auto be_end = ::st_Buffer_get_const_objects_end( eb );

        for( size_t ii = size_t{ 0 } ; be_it != be_end ; ++be_it )
        {
            type_id_t const type_id = ::st_Object_get_type_id( be_it );

            if( type_id == ::st_OBJECT_TYPE_BEAM_MONITOR )
            {
                ::st_BeamMonitor const& cmp_beam_monitor =
                    cmp_beam_monitors.at( ii++ );

                ::st_BeamMonitor* beam_monitor =
                    reinterpret_cast< ::st_BeamMonitor* >(
                        static_cast< uintptr_t >( ::st_Object_get_begin_addr(
                            be_it ) ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_out_address( beam_monitor )
                             == addr_t{ 0 } );

                ASSERT_TRUE( ::st_BeamMonitor_get_num_stores( beam_monitor ) ==
                             ::st_BeamMonitor_get_num_stores( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_skip( beam_monitor ) ==
                             ::st_BeamMonitor_get_skip( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_start( beam_monitor ) ==
                             ::st_BeamMonitor_get_start( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_is_rolling( beam_monitor ) ==
                             ::st_BeamMonitor_is_rolling( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_is_turn_ordered( beam_monitor ) ==
                             ::st_BeamMonitor_is_turn_ordered( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_min_particle_id( beam_monitor ) <=
                             static_cast< mon_index_t >( min_particle_id ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_max_particle_id( beam_monitor ) >=
                             static_cast< mon_index_t >( max_particle_id ) );
            }
        }

        ::st_ClArgument* beam_elements_arg =
            ::st_ClArgument_new_from_buffer( eb, context );

        ::st_ClArgument* out_buffer_arg =
            ::st_ClArgument_new_from_buffer( out_buffer, context );

        ASSERT_TRUE( beam_elements_arg != nullptr );
        ASSERT_TRUE( out_buffer_arg    != nullptr );

        ASSERT_TRUE( 0 == ::st_ClContext_assign_beam_monitor_out_buffer(
            context, beam_elements_arg, out_buffer_arg, 0u ) );

        ASSERT_TRUE( ::st_ClArgument_read( beam_elements_arg, eb ) );

        be_it  = ::st_Buffer_get_const_objects_begin( eb );
        be_end = ::st_Buffer_get_const_objects_end( eb );

        for( size_t ii = size_t{ 0 } ; be_it != be_end ; ++be_it )
        {
            type_id_t const type_id = ::st_Object_get_type_id( be_it );

            if( type_id == ::st_OBJECT_TYPE_BEAM_MONITOR )
            {
                ::st_BeamMonitor const& cmp_beam_monitor =
                    cmp_beam_monitors.at( ii++ );

                ::st_BeamMonitor* beam_monitor =
                    reinterpret_cast< ::st_BeamMonitor* >(
                        static_cast< uintptr_t >( ::st_Object_get_begin_addr(
                            be_it ) ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_out_address( beam_monitor )
                             != addr_t{ 0 } );

                ASSERT_TRUE( ::st_BeamMonitor_get_num_stores( beam_monitor ) ==
                             ::st_BeamMonitor_get_num_stores( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_skip( beam_monitor ) ==
                             ::st_BeamMonitor_get_skip( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_start( beam_monitor ) ==
                             ::st_BeamMonitor_get_start( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_is_rolling( beam_monitor ) ==
                             ::st_BeamMonitor_is_rolling( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_is_turn_ordered( beam_monitor ) ==
                             ::st_BeamMonitor_is_turn_ordered( &cmp_beam_monitor ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_min_particle_id( beam_monitor ) <=
                             static_cast< mon_index_t >( min_particle_id ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_max_particle_id( beam_monitor ) >=
                             static_cast< mon_index_t >( max_particle_id ) );
            }
        }

        ASSERT_TRUE( 0 == ::st_ClContext_clear_beam_monitor_out_assignment(
            context, beam_elements_arg ) );

        ASSERT_TRUE( ::st_ClArgument_read( beam_elements_arg, eb ) );

        be_it  = ::st_Buffer_get_const_objects_begin( eb );
        be_end = ::st_Buffer_get_const_objects_end( eb );

        for( size_t ii = size_t{ 0 } ; be_it != be_end ; ++be_it )
        {
            type_id_t const type_id = ::st_Object_get_type_id( be_it );

            if( type_id == ::st_OBJECT_TYPE_BEAM_MONITOR )
            {
                ::st_BeamMonitor const& cmp_beam_monitor =
                    cmp_beam_monitors.at( ii++ );

                ::st_BeamMonitor* beam_monitor =
                    reinterpret_cast< ::st_BeamMonitor* >(
                        static_cast< uintptr_t >( ::st_Object_get_begin_addr(
                            be_it ) ) );

                ASSERT_TRUE( ::st_BeamMonitor_get_out_address( beam_monitor )
                             == addr_t{ 0 } );

                ASSERT_TRUE( 0 == ::st_BeamMonitor_compare_values(
                    beam_monitor, &cmp_beam_monitor ) );
            }
        }

        ::st_ClArgument_delete( beam_elements_arg );
        ::st_ClArgument_delete( out_buffer_arg );

        ::st_ClContext_delete( context );
        context = nullptr;

        ::st_Buffer_delete( pb );
        ::st_Buffer_delete( out_buffer );
    }

    ::st_Buffer_delete( eb );
}


TEST( C99_OpenCLBeamMonitorTests, TrackingAndTurnByTurnIODebug )
{
    using real_t          = ::st_particle_real_t;
    using part_index_t    = ::st_particle_index_t;
    using size_t          = ::st_buffer_size_t;
    using nturn_t         = ::st_be_monitor_turn_t;
    using addr_t          = ::st_be_monitor_addr_t;
    using turn_dist_t     = std::uniform_int_distribution< nturn_t >;
    using chance_dist_t   = std::uniform_real_distribution< real_t >;

    std::mt19937_64::result_type const seed = 20181031u;

    std::mt19937_64 prng;
    prng.seed( seed );

    ::st_Buffer* eb = ::st_Buffer_new( 0u );
    ::st_Buffer* elem_by_elem_buffer = ::st_Buffer_new( 0u );

    size_t const NUM_BEAM_MONITORS  = size_t{  5 };
    size_t const NUM_DRIFTS         = size_t{ 10 };
    size_t const NUM_PARTICLES      = size_t{  2 };
    size_t const DRIFT_SEQU_LEN     = NUM_DRIFTS / NUM_BEAM_MONITORS;

    turn_dist_t num_stores_dist( 1, 8 );
    turn_dist_t start_dist( 0, 4 );
    turn_dist_t skip_dist( 1, 4 );

    static real_t const ABS_TOLERANCE =
        std::numeric_limits< real_t >::epsilon();

    chance_dist_t rolling_dist( 0., 1. );

    nturn_t max_num_turns  = nturn_t{ 0 };
    nturn_t max_start_turn = nturn_t{ 0 };

    for( size_t ii = size_t{ 0 } ; ii < NUM_BEAM_MONITORS ; ++ii )
    {
        ::st_BeamMonitor* be_monitor = ::st_BeamMonitor_add( eb,
            num_stores_dist( prng ), start_dist( prng ), skip_dist( prng ),
            addr_t{ 0 }, part_index_t{ 0 }, part_index_t{ 0 },
                bool{ rolling_dist( prng ) >= 0.5 }, true );

        ASSERT_TRUE( be_monitor != nullptr );

        nturn_t const num_stores =
            ::st_BeamMonitor_get_num_stores( be_monitor );

        ASSERT_TRUE( num_stores > nturn_t{ 0 } );

        nturn_t const skip  = ::st_BeamMonitor_get_skip( be_monitor );
        nturn_t const start = ::st_BeamMonitor_get_start( be_monitor );
        nturn_t const n     = num_stores * skip;

        if( max_num_turns  < n     ) max_num_turns  = n;
        if( max_start_turn < start ) max_start_turn = start;

        for( size_t jj = size_t{ 0 } ; jj < DRIFT_SEQU_LEN ; ++jj )
        {
            ::st_Drift*  drift = ::st_Drift_add( eb, real_t{ 1.0 } );
            ASSERT_TRUE( drift != nullptr );
        }
    }

    /* --------------------------------------------------------------------- */

    ASSERT_TRUE( max_num_turns > nturn_t{ 0 } );
    nturn_t const NUM_TURNS = max_start_turn + 2 * max_num_turns;

    ::st_Buffer* pb = ::st_Buffer_new( 0u );
    ::st_Particles* particles = ::st_Particles_new( pb, NUM_PARTICLES );
    ::st_Particles_realistic_init( particles );

    for( nturn_t ll = nturn_t{ 0 } ; ll < NUM_TURNS ; ++ll )
    {
        ASSERT_TRUE( 0 == ::st_Track_all_particles_append_element_by_element(
            particles, 0u, eb, elem_by_elem_buffer ) );

        ::st_Track_all_particles_increment_at_turn( particles, 0u );
    }

    ::st_Particles* final_state = ::st_Particles_add_copy(
        elem_by_elem_buffer, particles );

    ASSERT_TRUE( final_state );

    ::st_Buffer_delete( pb );
    particles = nullptr;
    pb = nullptr;

    /* --------------------------------------------------------------------- */
    /* get number of available OpenCL Nodes: */

    ::st_ClContext* context = ::st_ClContext_create();

    ASSERT_TRUE( context != nullptr );

    size_t const num_available_nodes =
        ::st_ClContextBase_get_num_available_nodes( context );

    ::st_ClContext_delete( context );
    context = nullptr;

    if( num_available_nodes > size_t{ 0 } )
    {
        for( size_t node_index = size_t{ 0 } ;
                node_index < num_available_nodes ; ++node_index )
        {
            context = ::st_ClContext_create();
            ::st_ClContextBase_enable_debug_mode( context );
            ::st_ClContext_disable_optimized_tracking_by_default( context );

            ASSERT_TRUE(  ::st_ClContextBase_is_debug_mode_enabled( context ) );
            ASSERT_TRUE( !::st_ClContext_uses_optimized_tracking_by_default( context ) );
            ASSERT_TRUE(  ::st_ClContextBase_select_node_by_index( context, node_index ) );

            ASSERT_TRUE( ::st_ClContextBase_has_selected_node( context ) );

            ::st_context_node_info_t const* node_info =
                ::st_ClContextBase_get_selected_node_info( context );

            ASSERT_TRUE( node_info != nullptr );
            ASSERT_TRUE( ::st_ClContextBase_has_remapping_kernel( context ) );

            char id_str[ 32 ];
            ::st_ClContextBase_get_selected_node_id_str( context, id_str, 32 );

            std::cout << "# ------------------------------------------------------"
                      << "--------------------------------------------------------"
                      << "\r\n"
                      << "# Run Test on :: \r\n"
                      << "# ID          :: " << id_str << "\r\n"
                      << "# NAME        :: "
                      << ::st_ComputeNodeInfo_get_name( node_info ) << "\r\n"
                      << "# PLATFORM    :: "
                      << ::st_ComputeNodeInfo_get_platform( node_info ) << "\r\n"
                      << "# Debug mode  :: "
                      << std::boolalpha
                      << ::st_ClContextBase_is_debug_mode_enabled( context ) << "\r\n"
                      << "# Optimized   :: "
                      << ::st_ClContext_uses_optimized_tracking_by_default( context )
                      << "\r\n"
                      << std::noboolalpha
                      << "# "
                      << std::endl;

            /* ----------------------------------------------------------------- */

            ( void )ABS_TOLERANCE;

            /*ASSERT_TRUE( sixtrack::tests::performBeamMonitorTrackingTest(
                context, eb, elem_by_elem_buffer, NUM_TURNS, ABS_TOLERANCE ) );*/

            ::st_ClContext_delete( context );
            context = nullptr;
        }

        for( size_t node_index = size_t{ 0 } ;
                node_index < num_available_nodes ; ++node_index )
        {
            context = ::st_ClContext_create();
            ::st_ClContextBase_disable_debug_mode( context );
            ::st_ClContext_disable_optimized_tracking_by_default( context );

            ASSERT_TRUE( !::st_ClContextBase_is_debug_mode_enabled( context ) );
            ASSERT_TRUE( !::st_ClContext_uses_optimized_tracking_by_default( context ) );
            ASSERT_TRUE(  ::st_ClContextBase_select_node_by_index( context, node_index ) );

            ASSERT_TRUE( ::st_ClContextBase_has_selected_node( context ) );

            ::st_context_node_info_t const* node_info =
                ::st_ClContextBase_get_selected_node_info( context );

            ASSERT_TRUE( node_info != nullptr );
            ASSERT_TRUE( ::st_ClContextBase_has_remapping_kernel( context ) );

            char id_str[ 32 ];
            ::st_ClContextBase_get_selected_node_id_str( context, id_str, 32 );

            std::cout << "# ------------------------------------------------------"
                      << "--------------------------------------------------------"
                      << "\r\n"
                      << "# Run Test on :: \r\n"
                      << "# ID          :: " << id_str << "\r\n"
                      << "# NAME        :: "
                      << ::st_ComputeNodeInfo_get_name( node_info ) << "\r\n"
                      << "# PLATFORM    :: "
                      << ::st_ComputeNodeInfo_get_platform( node_info ) << "\r\n"
                      << "# Debug mode  :: "
                      << std::boolalpha
                      << ::st_ClContextBase_is_debug_mode_enabled( context ) << "\r\n"
                      << "# Optimized   :: "
                      << ::st_ClContext_uses_optimized_tracking_by_default( context )
                      << "\r\n"
                      << std::noboolalpha
                      << "# "
                      << std::endl;

            /* ----------------------------------------------------------------- */

            /*ASSERT_TRUE( sixtrack::tests::performBeamMonitorTrackingTest(
                context, eb, elem_by_elem_buffer, NUM_TURNS, ABS_TOLERANCE ) );*/
            ::st_ClContext_delete( context );
            context = nullptr;
        }

        for( size_t node_index = size_t{ 0 } ;
                node_index < num_available_nodes ; ++node_index )
        {
            context = ::st_ClContext_create();
            ::st_ClContextBase_enable_debug_mode( context );
            ::st_ClContext_enable_optimized_tracking_by_default( context );

            ASSERT_TRUE(  ::st_ClContextBase_is_debug_mode_enabled( context ) );
            ASSERT_TRUE(  ::st_ClContext_uses_optimized_tracking_by_default( context ) );
            ASSERT_TRUE(  ::st_ClContextBase_select_node_by_index( context, node_index ) );

            ASSERT_TRUE( ::st_ClContextBase_has_selected_node( context ) );

            ::st_context_node_info_t const* node_info =
                ::st_ClContextBase_get_selected_node_info( context );

            ASSERT_TRUE( node_info != nullptr );
            ASSERT_TRUE( ::st_ClContextBase_has_remapping_kernel( context ) );

            char id_str[ 32 ];
            ::st_ClContextBase_get_selected_node_id_str( context, id_str, 32 );

            std::cout << "# ------------------------------------------------------"
                      << "--------------------------------------------------------"
                      << "\r\n"
                      << "# Run Test on :: \r\n"
                      << "# ID          :: " << id_str << "\r\n"
                      << "# NAME        :: "
                      << ::st_ComputeNodeInfo_get_name( node_info ) << "\r\n"
                      << "# PLATFORM    :: "
                      << ::st_ComputeNodeInfo_get_platform( node_info ) << "\r\n"
                      << "# Debug mode  :: "
                      << std::boolalpha
                      << ::st_ClContextBase_is_debug_mode_enabled( context ) << "\r\n"
                      << "# Optimized   :: "
                      << ::st_ClContext_uses_optimized_tracking_by_default( context )
                      << "\r\n"
                      << std::noboolalpha
                      << "# "
                      << std::endl;

            /* ----------------------------------------------------------------- */

            /*ASSERT_TRUE( sixtrack::tests::performBeamMonitorTrackingTest(
                context, eb, elem_by_elem_buffer, NUM_TURNS, ABS_TOLERANCE ) );*/

            ::st_ClContext_delete( context );
            context = nullptr;
        }

        for( size_t node_index = size_t{ 0 } ;
                node_index < num_available_nodes ; ++node_index )
        {
            context = ::st_ClContext_create();
            ::st_ClContextBase_disable_debug_mode( context );
            ::st_ClContext_enable_optimized_tracking_by_default( context );

            ASSERT_TRUE( !::st_ClContextBase_is_debug_mode_enabled( context ) );
            ASSERT_TRUE(  ::st_ClContext_uses_optimized_tracking_by_default( context ) );
            ASSERT_TRUE(  ::st_ClContextBase_select_node_by_index( context, node_index ) );

            ASSERT_TRUE( ::st_ClContextBase_has_selected_node( context ) );

            ::st_context_node_info_t const* node_info =
                ::st_ClContextBase_get_selected_node_info( context );

            ASSERT_TRUE( node_info != nullptr );
            ASSERT_TRUE( ::st_ClContextBase_has_remapping_kernel( context ) );

            char id_str[ 32 ];
            ::st_ClContextBase_get_selected_node_id_str( context, id_str, 32 );

            std::cout << "# ------------------------------------------------------"
                      << "--------------------------------------------------------"
                      << "\r\n"
                      << "# Run Test on :: \r\n"
                      << "# ID          :: " << id_str << "\r\n"
                      << "# NAME        :: "
                      << ::st_ComputeNodeInfo_get_name( node_info ) << "\r\n"
                      << "# PLATFORM    :: "
                      << ::st_ComputeNodeInfo_get_platform( node_info ) << "\r\n"
                      << "# Debug mode  :: "
                      << std::boolalpha
                      << ::st_ClContextBase_is_debug_mode_enabled( context ) << "\r\n"
                      << "# Optimized   :: "
                      << ::st_ClContext_uses_optimized_tracking_by_default( context )
                      << "\r\n"
                      << std::noboolalpha
                      << "# "
                      << std::endl;

            /* ----------------------------------------------------------------- */

            /*ASSERT_TRUE( sixtrack::tests::performBeamMonitorTrackingTest(
                context, eb, elem_by_elem_buffer, NUM_TURNS, ABS_TOLERANCE ) );*/

            ::st_ClContext_delete( context );
            context = nullptr;
        }
    }
    else
    {
        std::cout << "Unable to run unit-test -> no OpenCL Nodes found \r\n"
                  << "--> skipping!" << std::endl;
    }

    /* --------------------------------------------------------------------- */

    ::st_Buffer_delete( eb );
    ::st_Buffer_delete( elem_by_elem_buffer );
}


namespace sixtrack
{
    namespace tests
    {
        bool performBeamMonitorTrackingTest(
            ::st_ClContext* SIXTRL_RESTRICT context,
            ::st_Buffer* SIXTRL_RESTRICT eb,
            ::st_Buffer const* SIXTRL_RESTRICT elem_by_elem_buffer,
            ::st_buffer_size_t const num_turns,
            double const abs_tolerance )
        {
            using size_t = ::st_buffer_size_t;
            using part_index_t    = ::st_particle_index_t;
            using nturn_t         = ::st_be_monitor_turn_t;
            using addr_t          = ::st_be_monitor_addr_t;
            using type_id_t       = ::st_object_type_id_t;
            using ptr_particles_t = ::st_Particles const*;
            using beam_monitor_t  = ::st_BeamMonitor;
            using ptr_const_mon_t = beam_monitor_t const*;
            using num_elem_t      = ::st_particle_num_elements_t;

            bool success = false;
            size_t const num_elem_by_elem_objects =
                ::st_Buffer_get_num_of_objects( elem_by_elem_buffer );

            ::st_Buffer* pb = ::st_Buffer_new( 0u );
            ::st_Buffer* out_buffer = ::st_Buffer_new( 0u );
            ::st_Buffer* cmp_particles_buffer = ::st_Buffer_new( 0u );

            if( ( num_elem_by_elem_objects > size_t{ 0 } ) &&
                ( pb != nullptr ) && ( out_buffer != nullptr ) &&
                ( cmp_particles_buffer != nullptr ) &&
                ( elem_by_elem_buffer != nullptr ) &&
                ( num_turns > size_t{ 0 } ) &&
                ( context != nullptr ) &&
                ( ::st_ClContextBase_has_selected_node( context ) ) &&
                ( abs_tolerance >= double{ 0. } ) )
            {
                success = true;
            }

            ::st_Particles const* initial_state = nullptr;
            ::st_Particles const* final_state   = nullptr;
            ::st_Particles const* particles     = nullptr;

            if( success )
            {
                initial_state = ::st_Particles_buffer_get_const_particles(
                    elem_by_elem_buffer, 0u );

                final_state = ::st_Particles_buffer_get_const_particles(
                    elem_by_elem_buffer, num_elem_by_elem_objects - size_t{ 1 } );

                particles = ::st_Particles_add_copy( pb, initial_state );
            }

            success = ( ( initial_state != nullptr ) &&
                        ( final_state   != nullptr ) &&
                        ( particles     != nullptr ) );

            if( success )
            {
                success = ( 0 == ::st_BeamMonitor_prepare_particles_out_buffer(
                    eb, out_buffer, particles, 0u ) );
            }

            /* ------------------------------------------------------------------ */
            /* Create ClArguments for the beam elements and the particles buffer */

            ::st_ClArgument* particles_buffer_arg = nullptr;
            ::st_ClArgument* beam_elements_arg    = nullptr;
            ::st_ClArgument* out_buffer_arg       = nullptr;

            if( success )
            {
                particles_buffer_arg = ::st_ClArgument_new_from_buffer( pb, context );
                beam_elements_arg = ::st_ClArgument_new_from_buffer( eb, context );
                out_buffer_arg = ::st_ClArgument_new_from_buffer( out_buffer, context );
            }

            success = ( ( particles_buffer_arg != nullptr ) &&
                        ( beam_elements_arg    != nullptr ) &&
                        ( out_buffer_arg       != nullptr ) );

            /* ----------------------------------------------------------------- */
            /* Track for num-turns without assigned beam-monitors -> should
             * not change the correctness of tracking at all */

            if( success )
            {
                success = ( 0 == ::st_ClContext_track(
                    context, particles_buffer_arg, beam_elements_arg, num_turns ) );
            }

            if( success )
            {
                success = ::st_ClArgument_read( particles_buffer_arg, pb );
                particles = ::st_Particles_buffer_get_particles( pb, 0u );
                success &= ( particles != nullptr );
            }

            if( ( success ) &&
                ( 0 != ::st_Particles_compare_values_with_treshold(
                    particles, final_state, abs_tolerance ) ) )
            {
                ::st_Buffer* diff_buffer = ::st_Buffer_new( 0u );
                ::st_Particles* diff = ::st_Particles_new( diff_buffer,
                    ::st_Particles_get_num_of_particles( particles ) );

                success = ( diff_buffer != nullptr );
                ::st_Particles_calculate_difference( particles, final_state, diff );

                std::cout << std::endl << "tracked = " << std::endl;
                ::st_Particles_print_out( particles );

                std::cout << std::endl << "final_state = " << std::endl;
                ::st_Particles_print_out( final_state );

                std::cout << std::endl << "diff = " << std::endl;
                ::st_Particles_print_out( diff );

                ::st_Buffer_delete( diff_buffer );
                diff_buffer = nullptr;

                success = false;
            }

            if( success ) success = ::st_Particles_copy(
                ::st_Particles_buffer_get_particles( pb, 0u ), initial_state );

            if( success ) success = ::st_ClArgument_write( particles_buffer_arg, pb );

            /* ------------------------------------------------------------------ */
            /* Now assign the out_buffer buffer to the beam monitors */

            if( success )
            {
                success &= ( 0 == ::st_ClContext_assign_beam_monitor_out_buffer(
                context, beam_elements_arg, out_buffer_arg, 0u ) );
            }

            /* ------------------------------------------------------------------ */
            /* Repeat the tracking -> we should now get the output in the
             * out_buffer buffer due by virtue of the beam monitors */

            if( success )
            {
                success  = ( 0 == ::st_ClContext_track(
                    context, particles_buffer_arg, beam_elements_arg, num_turns ) );

                success &= ::st_ClArgument_read( particles_buffer_arg, pb );
                success &= ::st_ClArgument_read( out_buffer_arg, out_buffer );
                success &= ::st_ClArgument_read( beam_elements_arg, eb );

                particles = ::st_Particles_buffer_get_particles( pb, 0u );
                success &= ( particles != nullptr );
            }

            if( success )
            {
                if( 0 != ::st_Particles_compare_values_with_treshold(
                    particles, final_state, abs_tolerance ) )
                {
                    ::st_Buffer* diff_buffer = ::st_Buffer_new( 0u );
                    ::st_Particles* diff = ::st_Particles_new( diff_buffer,
                        ::st_Particles_get_num_of_particles( particles ) );

                    success = ( diff_buffer != nullptr );
                    ::st_Particles_calculate_difference( particles, final_state, diff );

                    std::cout << std::endl << "tracked 2 = " << std::endl;
                    ::st_Particles_print_out( particles );

                    std::cout << std::endl << "final_state = " << std::endl;
                    ::st_Particles_print_out( final_state );

                    std::cout << std::endl << "diff = " << std::endl;
                    ::st_Particles_print_out( diff );

                    ::st_Buffer_delete( diff_buffer );
                    diff_buffer = nullptr;

                    success = false;
                }
            }

            /* ------------------------------------------------------------------ */
            /* Re-Assign the Io buffer to the beam-monitors -> this allows
             * easier read-out */

            if( success )
            {
                success = ( 0 == ::st_BeamMonitor_assign_particles_out_buffer(
                eb, out_buffer, 0u ) );
            }

            /* ----------------------------------------------------------------- */
            /* Compare beam-monitor dump values with the element by element
             * dump gathered prior to tracking with this device on the CPU */

            ::st_Particles* cmp_particles = nullptr;

            if( success )
            {
                cmp_particles = ::st_Particles_new( cmp_particles_buffer,
                    ::st_Particles_get_num_of_particles( initial_state ) );
            }

            success &= ( cmp_particles != nullptr );

            size_t jj = size_t{ 0 };

            ::st_Object const* obj_begin = ::st_Buffer_get_const_objects_begin( eb );
            ::st_Object const* obj_end   = ::st_Buffer_get_const_objects_end( eb );

            nturn_t const NUM_TURNS = static_cast< nturn_t >( num_turns );

            for( nturn_t kk = nturn_t{ 0 } ; kk < NUM_TURNS ; ++kk )
            {
                ::st_Object const* obj_it = obj_begin;

                for( ; obj_it != obj_end ; ++obj_it, ++jj )
                {
                    if( jj >= num_elem_by_elem_objects )
                    {
                        success = false;
                        break;
                    }

                    ::st_Particles const* elem_by_elem_particles =
                        ::st_Particles_buffer_get_const_particles(
                            elem_by_elem_buffer, jj );

                    if( elem_by_elem_particles == nullptr )
                    {
                        success = false;
                        break;
                    }

                    if( ::st_Object_get_type_id( obj_it ) == ::st_OBJECT_TYPE_BEAM_MONITOR )
                    {
                        ptr_const_mon_t beam_monitor = reinterpret_cast< ptr_const_mon_t >(
                                ::st_Object_get_const_begin_ptr( obj_it ) );

                        if( ::st_BeamMonitor_get_out_address( beam_monitor )
                            == addr_t{ 0 } )
                        {
                            success = false;
                            break;
                        }

                        if( !::st_BeamMonitor_has_turn_stored(
                                beam_monitor, kk, NUM_TURNS ) )
                        {
                            continue;
                        }

                        success &= ( ::st_BeamMonitor_get_start( beam_monitor ) <= kk );
                        success &= ( ( ( kk - ::st_BeamMonitor_get_start( beam_monitor ) )
                            % ::st_BeamMonitor_get_skip( beam_monitor ) ) == nturn_t{ 0 } );

                        if( !success ) break;

                        ptr_particles_t out_particles = reinterpret_cast< ptr_particles_t >(
                            static_cast< uintptr_t >( ::st_BeamMonitor_get_out_address(
                                beam_monitor ) ) );

                        success &= ( elem_by_elem_particles != nullptr );
                        if( !success ) break;

                        size_t const NUM_PARTICLES =
                            ::st_Particles_get_num_of_particles( particles );

                        for( size_t ll = size_t{ 0 } ; ll < NUM_PARTICLES ; ++ll )
                        {
                            part_index_t const particle_id =
                                ::st_Particles_get_particle_id_value( particles, ll );

                            num_elem_t const stored_particle_id =
                                ::st_BeamMonitor_get_store_particle_index(
                                    beam_monitor, kk, particle_id );

                            success &= ( stored_particle_id >= num_elem_t{ 0 } );
                            success &= ( ::st_Particles_copy_single( cmp_particles, ll,
                                            out_particles, stored_particle_id ) );

                            if( !success ) break;
                        }

                        if( !success ) break;

                        if( ( success ) &&
                            ( 0 != ::st_Particles_compare_values_with_treshold(
                                cmp_particles, elem_by_elem_particles, abs_tolerance ) ) )
                        {
                            std::cout << "jj = " << jj << " / kk = " << kk << std::endl;

                            ::st_Buffer* diff_buffer = ::st_Buffer_new( 0u );
                            ::st_Particles* diff =
                                ::st_Particles_new( diff_buffer, NUM_PARTICLES );

                            success &= ( diff != nullptr );

                            ::st_Particles_calculate_difference(
                                cmp_particles, elem_by_elem_particles, diff );

                            std::cout << "cmp_particles: " << std::endl;
                            ::st_Particles_print_out( cmp_particles );

                            std::cout << std::endl << "elem_by_elem_particles: "
                                        << std::endl;

                            ::st_Particles_print_out( elem_by_elem_particles );

                            std::cout << std::endl << "diff: " << std::endl;
                            ::st_Particles_print_out( diff );

                            ::st_Buffer_delete( diff_buffer );
                            diff_buffer = nullptr;

                            success = false;
                        }
                    }

                    if( !success ) break;
                }
            }

            /* ----------------------------------------------------------------- */
            /* Clear assigned output buffer: */

            if( success )
            {
                success = ( 0 == ::st_ClContext_clear_beam_monitor_out_assignment(
                    context, beam_elements_arg ) );

                success &= ( ::st_ClArgument_read( beam_elements_arg, eb ) );
            }

            if( success )
            {
                ::st_Object const* obj_it = ::st_Buffer_get_const_objects_begin( eb );
                obj_end = ::st_Buffer_get_const_objects_end( eb );

                for(  ; obj_it != obj_end ; ++obj_it )
                {
                    type_id_t const type_id = ::st_Object_get_type_id( obj_it );

                    if( type_id == ::st_OBJECT_TYPE_BEAM_MONITOR )
                    {
                        ::st_BeamMonitor const* beam_monitor =
                            reinterpret_cast< ::st_BeamMonitor const* >( static_cast<
                                uintptr_t >( ::st_Object_get_begin_addr( obj_it ) ) );

                        success &= ( ::st_BeamMonitor_get_out_address( beam_monitor )
                            == addr_t{ 0 } );
                    }

                    if( !success ) break;
                }
            }

            ::st_ClArgument_delete( beam_elements_arg );
            ::st_ClArgument_delete( particles_buffer_arg );
            ::st_ClArgument_delete( out_buffer_arg );

            ::st_Buffer_delete( pb );
            ::st_Buffer_delete( out_buffer );
            ::st_Buffer_delete( cmp_particles_buffer );

            return success;
        }
    }
}

/* end: tests/sixtracklib/opencl/test_be_monitor_opencl_c99.cpp */