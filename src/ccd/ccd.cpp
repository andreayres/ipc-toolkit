#include <ipc/ccd/ccd.hpp>

#include <algorithm> // std::min/max
#include <array>

#ifdef IPC_TOOLKIT_WITH_CORRECT_CCD
#include <tight_inclusion/inclusion_ccd.hpp>
#else
#include <CTCD.h>
#endif

#include <ipc/distance/edge_edge.hpp>
#include <ipc/distance/point_triangle.hpp>

namespace ipc {

#ifdef IPC_TOOLKIT_WITH_CORRECT_CCD
static const int TIGHT_INCLUSION_CCD_TYPE = 1;
#endif

bool ccd_strategy(
    const std::function<bool(
        double /*min_distance*/, bool /*no_zero_toi*/, double& /*toi*/)>& ccd,
    const double initial_distance,
    const double conservative_rescaling,
    double& toi)
{
    static constexpr double SMALL_TOI = 1e-6;

    if (initial_distance == 0) {
        IPC_LOG(warn("Initial distance is 0, returning toi=0!"));
        toi = 0;
        return true;
    }

    double min_distance = (1.0 - conservative_rescaling) * initial_distance;
    assert(min_distance < initial_distance);

    bool is_impacting = ccd(min_distance, /*no_zero_toi=*/false, toi);

    if (is_impacting && toi < SMALL_TOI) {
        bool is_impacting = ccd(/*min_distance=*/0, /*no_zero_toi=*/true, toi);

        if (is_impacting) {
            toi *= conservative_rescaling;
            assert(toi != 0);
        }
    }

    return is_impacting;
}

bool point_point_ccd(
    const Eigen::Vector3d& p0_t0,
    const Eigen::Vector3d& p1_t0,
    const Eigen::Vector3d& p0_t1,
    const Eigen::Vector3d& p1_t1,
    double& toi,
    const double tmax,
    const double tolerance,
    const long max_iterations,
    const double conservative_rescaling)
{
    assert(tmax >= 0 && tmax <= 1.0);

    const auto ccd = [&](double min_distance, bool no_zero_toi,
                         double& toi) -> bool {
#ifdef IPC_TOOLKIT_WITH_CORRECT_CCD
        double output_tolerance;
        // NOTE: Use degenerate edge-edge
        return inclusion_ccd::edgeEdgeCCD_double(
            p0_t0, p0_t0, p1_t0, p1_t0, p0_t1, p0_t1, p1_t1, p1_t1,
            { { -1, -1, -1 } }, // rounding error (auto)
            min_distance,       // minimum separation distance
            toi,                // time of impact
            tolerance,          // delta
            tmax,               // maximum time to check
            max_iterations,     // maximum number of iterations
            output_tolerance,   // delta_actual
            TIGHT_INCLUSION_CCD_TYPE, no_zero_toi);
#else
        return CTCD::vertexVertexCTCD(
            p0_t0, p1_t0, p0_t1, p1_t1, min_distance, toi);
#endif
    };

    double initial_distance = sqrt(point_point_distance(p0_t0, p1_t0));

    return ccd_strategy(ccd, initial_distance, conservative_rescaling, toi);
}

inline Eigen::Vector3d to_3D(const Eigen::Vector2d& v)
{
    return Eigen::Vector3d(v.x(), v.y(), 0);
}

bool point_edge_ccd_2D(
    const Eigen::Vector2d& p_t0,
    const Eigen::Vector2d& e0_t0,
    const Eigen::Vector2d& e1_t0,
    const Eigen::Vector2d& p_t1,
    const Eigen::Vector2d& e0_t1,
    const Eigen::Vector2d& e1_t1,
    double& toi,
    const double tmax,
    const double tolerance,
    const long max_iterations,
    const double conservative_rescaling)
{
#ifndef IPC_TOOLKIT_WITH_CORRECT_CCD
    inexact_point_edge_ccd_2D(
        p_t0, e0_t0, e1_t0, p_t1, e0_t1, e1_t1, toi, conservative_rescaling);
#else
    assert(0 <= tmax && tmax <= 1.0);

    Eigen::Vector3d p_t0_3D = to_3D(p_t0);
    Eigen::Vector3d e0_t0_3D = to_3D(e0_t0);
    Eigen::Vector3d e1_t0_3D = to_3D(e1_t0);
    Eigen::Vector3d p_t1_3D = to_3D(p_t1);
    Eigen::Vector3d e0_t1_3D = to_3D(e0_t1);
    Eigen::Vector3d e1_t1_3D = to_3D(e1_t1);

    const auto ccd = [&](double min_distance, bool no_zero_toi,
                         double& toi) -> bool {
        double output_tolerance;
        // NOTE: Use degenerate edge-edge
        return inclusion_ccd::edgeEdgeCCD_double(
            p_t0_3D, p_t0_3D, e0_t0_3D, e1_t0_3D, //
            p_t1_3D, p_t1_3D, e0_t1_3D, e1_t1_3D,
            { { -1, -1, -1 } }, // rounding error (auto)
            min_distance,       // minimum separation distance
            toi,                // time of impact
            tolerance,          // delta
            tmax,               // maximum time to check
            max_iterations,     // maximum number of iterations
            output_tolerance,   // delta_actual
            TIGHT_INCLUSION_CCD_TYPE, no_zero_toi);
    };

    double initial_distance = sqrt(point_edge_distance(p_t0, e0_t0, e1_t0));

    return ccd_strategy(ccd, initial_distance, conservative_rescaling, toi);
#endif
}

bool point_edge_ccd_3D(
    const Eigen::Vector3d& p_t0,
    const Eigen::Vector3d& e0_t0,
    const Eigen::Vector3d& e1_t0,
    const Eigen::Vector3d& p_t1,
    const Eigen::Vector3d& e0_t1,
    const Eigen::Vector3d& e1_t1,
    double& toi,
    const double tmax,
    const double tolerance,
    const long max_iterations,
    const double conservative_rescaling)
{
    assert(tmax >= 0 && tmax <= 1.0);

    const auto ccd = [&](double min_distance, bool no_zero_toi,
                         double& toi) -> bool {
#ifdef IPC_TOOLKIT_WITH_CORRECT_CCD
        double output_tolerance = tolerance;
        // NOTE: Use degenerate edge-edge
        return inclusion_ccd::edgeEdgeCCD_double(
            p_t0, p_t0, e0_t0, e1_t0, p_t1, p_t1, e0_t1, e1_t1,
            { { -1, -1, -1 } }, // rounding error (auto)
            min_distance,       // minimum separation distance
            toi,                // time of impact
            tolerance,          // delta
            tmax,               // maximum time to check
            max_iterations,     // maximum number of iterations
            output_tolerance,   // delta_actual
            TIGHT_INCLUSION_CCD_TYPE, no_zero_toi);
#else
        return CTCD::vertexEdgeCTCD(
            p_t0, e0_t0, e1_t0, p_t1, e0_t1, e1_t1, min_distance, toi);
#endif
    };

    double initial_distance = sqrt(point_edge_distance(p_t0, e0_t0, e1_t0));

    return ccd_strategy(ccd, initial_distance, conservative_rescaling, toi);
}

bool point_edge_ccd(
    const VectorMax3d& p_t0,
    const VectorMax3d& e0_t0,
    const VectorMax3d& e1_t0,
    const VectorMax3d& p_t1,
    const VectorMax3d& e0_t1,
    const VectorMax3d& e1_t1,
    double& toi,
    const double tmax,
    const double tolerance,
    const long max_iterations,
    const double conservative_rescaling)
{
    int dim = p_t0.size();
    assert(e0_t0.size() == dim);
    assert(e1_t0.size() == dim);
    assert(p_t1.size() == dim);
    assert(e0_t1.size() == dim);
    assert(e1_t1.size() == dim);
    if (dim == 2) {
        return point_edge_ccd_2D(
            p_t0, e0_t0, e1_t0, p_t1, e0_t1, e1_t1, //
            toi, tmax, tolerance, max_iterations, conservative_rescaling);
    } else {
        return point_edge_ccd_3D(
            p_t0, e0_t0, e1_t0, p_t1, e0_t1, e1_t1, //
            toi, tmax, tolerance, max_iterations, conservative_rescaling);
    }
}

bool edge_edge_ccd(
    const Eigen::Vector3d& ea0_t0,
    const Eigen::Vector3d& ea1_t0,
    const Eigen::Vector3d& eb0_t0,
    const Eigen::Vector3d& eb1_t0,
    const Eigen::Vector3d& ea0_t1,
    const Eigen::Vector3d& ea1_t1,
    const Eigen::Vector3d& eb0_t1,
    const Eigen::Vector3d& eb1_t1,
    double& toi,
    const double tmax,
    const double tolerance,
    const long max_iterations,
    const double conservative_rescaling)
{
    assert(tmax >= 0 && tmax <= 1.0);

    const auto ccd = [&](double min_distance, bool no_zero_toi,
                         double& toi) -> bool {
#ifdef IPC_TOOLKIT_WITH_CORRECT_CCD
        double output_tolerance;
        return inclusion_ccd::edgeEdgeCCD_double(
            ea0_t0, ea1_t0, eb0_t0, eb1_t0, ea0_t1, ea1_t1, eb0_t1, eb1_t1,
            { { -1, -1, -1 } }, // rounding error (auto)
            min_distance,       // minimum separation distance
            toi,                // time of impact
            tolerance,          // delta
            tmax,               // maximum time to check
            max_iterations,     // maximum number of iterations
            output_tolerance,   // delta_actual
            TIGHT_INCLUSION_CCD_TYPE, no_zero_toi);
#else
        return CTCD::edgeEdgeCTCD(
            ea0_t0, ea1_t0, eb0_t0, eb1_t0, ea0_t1, ea1_t1, eb0_t1, eb1_t1,
            min_distance, toi);
#endif
    };

    double initial_distance =
        sqrt(edge_edge_distance(ea0_t0, ea1_t0, eb0_t0, eb1_t0));

    return ccd_strategy(ccd, initial_distance, conservative_rescaling, toi);
}

bool point_triangle_ccd(
    const Eigen::Vector3d& p_t0,
    const Eigen::Vector3d& t0_t0,
    const Eigen::Vector3d& t1_t0,
    const Eigen::Vector3d& t2_t0,
    const Eigen::Vector3d& p_t1,
    const Eigen::Vector3d& t0_t1,
    const Eigen::Vector3d& t1_t1,
    const Eigen::Vector3d& t2_t1,
    double& toi,
    const double tmax,
    const double tolerance,
    const long max_iterations,
    const double conservative_rescaling)
{
    assert(tmax >= 0 && tmax <= 1.0);

    const auto ccd = [&](double min_distance, bool no_zero_toi,
                         double& toi) -> bool {
#ifdef IPC_TOOLKIT_WITH_CORRECT_CCD
        double output_tolerance;
        return inclusion_ccd::vertexFaceCCD_double(
            p_t0, t0_t0, t1_t0, t2_t0, p_t1, t0_t1, t1_t1, t2_t1,
            { { -1, -1, -1 } }, // rounding error (auto)
            min_distance,       // minimum separation distance
            toi,                // time of impact
            tolerance,          // delta
            tmax,               // maximum time to check
            max_iterations,     // maximum number of iterations
            output_tolerance,   // delta_actual
            TIGHT_INCLUSION_CCD_TYPE, no_zero_toi);
#else
        return CTCD::vertexFaceCTCD(
            p_t0, t0_t0, t1_t0, t2_t0, p_t1, t0_t1, t1_t1, t2_t1, //
            min_distance, toi);
#endif
    };

    double initial_distance =
        sqrt(point_triangle_distance(p_t0, t0_t0, t1_t0, t2_t0));

    return ccd_strategy(ccd, initial_distance, conservative_rescaling, toi);
}

} // namespace ipc
