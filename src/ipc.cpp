#include <ipc/ipc.hpp>

#include <algorithm> // std::min/max

#define IPC_EARLIEST_TOI_USE_MUTEX
#ifdef IPC_EARLIEST_TOI_USE_MUTEX
#include <mutex>
#endif
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>

#include <igl/predicates/segment_segment_intersect.h>
#ifdef IPC_TOOLKIT_WITH_CUDA
#include <ccdgpu/helper.cuh>
#endif

#include <ipc/ccd/ccd.hpp>
#include <ipc/broad_phase/hash_grid.hpp>
#include <ipc/utils/local_to_global.hpp>
#include <ipc/utils/intersection.hpp>
#include <ipc/utils/world_bbox_diagonal_length.hpp>

namespace ipc {

double compute_barrier_potential(
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V,
    const Constraints& constraint_set,
    const double dhat)
{
    assert(V.rows() == mesh.num_vertices());

    if (constraint_set.empty()) {
        return 0;
    }

    const Eigen::MatrixXi& E = mesh.edges();
    const Eigen::MatrixXi& F = mesh.faces();

    tbb::enumerable_thread_specific<double> storage(0);

    tbb::parallel_for(
        tbb::blocked_range<size_t>(size_t(0), constraint_set.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            auto& local_potential = storage.local();
            for (size_t i = r.begin(); i < r.end(); i++) {
                // Quadrature weight is premultiplied by compute_potential
                local_potential +=
                    constraint_set[i].compute_potential(V, E, F, dhat);
            }
        });

    double potential = 0;
    for (const auto& local_potential : storage) {
        potential += local_potential;
    }
    return potential;
}

Eigen::VectorXd compute_barrier_potential_gradient(
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V,
    const Constraints& constraint_set,
    const double dhat)
{
    assert(V.rows() == mesh.num_vertices());

    if (constraint_set.empty()) {
        return Eigen::VectorXd::Zero(V.size());
    }

    const Eigen::MatrixXi& E = mesh.edges();
    const Eigen::MatrixXi& F = mesh.faces();

    int dim = V.cols();

    tbb::enumerable_thread_specific<Eigen::VectorXd> storage(
        Eigen::VectorXd::Zero(V.size()));

    tbb::parallel_for(
        tbb::blocked_range<size_t>(size_t(0), constraint_set.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            auto& local_grad = storage.local();
            for (size_t i = r.begin(); i < r.end(); i++) {
                local_gradient_to_global_gradient(
                    constraint_set[i].compute_potential_gradient(V, E, F, dhat),
                    constraint_set[i].vertex_indices(E, F), dim, local_grad);
            }
        });

    Eigen::VectorXd grad = Eigen::VectorXd::Zero(V.size());
    for (const auto& local_grad : storage) {
        grad += local_grad;
    }
    return grad;
}

Eigen::SparseMatrix<double> compute_barrier_potential_hessian(
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V,
    const Constraints& constraint_set,
    const double dhat,
    const bool project_hessian_to_psd)
{
    assert(V.rows() == mesh.num_vertices());

    if (constraint_set.empty()) {
        return Eigen::SparseMatrix<double>(V.size(), V.size());
    }

    const Eigen::MatrixXi& E = mesh.edges();
    const Eigen::MatrixXi& F = mesh.faces();

    int dim = V.cols();

    tbb::enumerable_thread_specific<std::vector<Eigen::Triplet<double>>>
        storage;

    tbb::parallel_for(
        tbb::blocked_range<size_t>(size_t(0), constraint_set.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            auto& local_hess_triplets = storage.local();

            for (size_t i = r.begin(); i < r.end(); i++) {
                local_hessian_to_global_triplets(
                    constraint_set[i].compute_potential_hessian(
                        V, E, F, dhat, project_hessian_to_psd),
                    constraint_set[i].vertex_indices(E, F), dim,
                    local_hess_triplets);
            }
        });

    Eigen::SparseMatrix<double> hess(V.size(), V.size());
    for (const auto& local_hess_triplets : storage) {
        Eigen::SparseMatrix<double> local_hess(V.size(), V.size());
        local_hess.setFromTriplets(
            local_hess_triplets.begin(), local_hess_triplets.end());
        hess += local_hess;
    }
    return hess;
}

Eigen::SparseMatrix<double> compute_barrier_shape_derivative(
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V,
    const Constraints& constraint_set,
    const double dhat)
{
    Eigen::SparseMatrix<double> shape_derivative =
        compute_barrier_potential_hessian(mesh, V, constraint_set, dhat, false);

    for (int i = 0; i < constraint_set.size(); i++) {
        const CollisionConstraint& constraint = constraint_set[i];
        assert(constraint.weight_gradient.size() == V.size());

        VectorMax12d local_barrier_grad = constraint.compute_potential_gradient(
            V, mesh.edges(), mesh.faces(), dhat);
        assert(constraint.weight != 0);
        local_barrier_grad.array() /= constraint.weight;

        Eigen::SparseVector<double> barrier_grad(V.size());
        barrier_grad.reserve(local_barrier_grad.size());
        local_gradient_to_global_gradient(
            local_barrier_grad,
            constraint.vertex_indices(mesh.edges(), mesh.faces()), V.cols(),
            barrier_grad);

        shape_derivative +=
            barrier_grad * constraint.weight_gradient.transpose();
    }

    return shape_derivative;
}

///////////////////////////////////////////////////////////////////////////////

bool is_step_collision_free(
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V0,
    const Eigen::MatrixXd& V1,
    const BroadPhaseMethod& method,
    const double tolerance,
    const long max_iterations)
{
    assert(V0.rows() == mesh.num_vertices());
    assert(V1.rows() == mesh.num_vertices());

    // Broad phase
    Candidates candidates;
    construct_collision_candidates(
        mesh, V0, V1, candidates, /*inflation_radius=*/0, method);

    // Narrow phase
    return is_step_collision_free(
        candidates, mesh, V0, V1, tolerance, max_iterations);
}

bool is_step_collision_free(
    const Candidates& candidates,
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V0,
    const Eigen::MatrixXd& V1,
    const double tolerance,
    const long max_iterations)
{
    assert(V0.rows() == mesh.num_vertices());
    assert(V1.rows() == mesh.num_vertices());

    const Eigen::MatrixXi& E = mesh.edges();
    const Eigen::MatrixXi& F = mesh.faces();

    // Narrow phase
    for (size_t i = 0; i < candidates.size(); i++) {
        double toi;
        bool is_collision = candidates[i].ccd(
            V0, V1, E, F, toi, /*tmax=*/1.0, tolerance, max_iterations);

        if (is_collision) {
            return false;
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

double compute_collision_free_stepsize(
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V0,
    const Eigen::MatrixXd& V1,
    const BroadPhaseMethod& method,
    const double tolerance,
    const long max_iterations)
{
    assert(V0.rows() == mesh.num_vertices());
    assert(V1.rows() == mesh.num_vertices());
    const Eigen::MatrixXi& E = mesh.edges();
    const Eigen::MatrixXi& F = mesh.faces();

#ifdef IPC_TOOLKIT_WITH_CUDA
    if (method == BroadPhaseMethod::SWEEP_AND_TINIEST_QUEUE_GPU) {
        double min_distance = 0; // TODO
        const double step_size = ccd::gpu::compute_toi_strategy(
            V0, V1, E, F, max_iterations, min_distance, tolerance);
        if (step_size < 1.0) {
            return 0.8 * step_size;
        }
        return 1.0;
    }
#endif

    // Broad phase
    Candidates candidates;
    construct_collision_candidates(
        mesh, V0, V1, candidates, /*inflation_radius=*/0, method);

    // Narrow phase
    double step_size = compute_collision_free_stepsize(
        candidates, mesh, V0, V1, tolerance, max_iterations);

    return step_size;
}

double compute_collision_free_stepsize(
    const Candidates& candidates,
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V0,
    const Eigen::MatrixXd& V1,
    const double tolerance,
    const long max_iterations)
{
    assert(V0.rows() == mesh.num_vertices());
    assert(V1.rows() == mesh.num_vertices());
    const Eigen::MatrixXi& E = mesh.edges();
    const Eigen::MatrixXi& F = mesh.faces();

    if (candidates.empty()) {
        return 1; // No possible collisions, so can take full step.
    }

    // Narrow phase
#ifdef IPC_EARLIEST_TOI_USE_MUTEX
    double earliest_toi = 1;
    std::mutex earliest_toi_mutex;
#else
    tbb::enumerable_thread_specific<double> storage(1);
#endif

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, candidates.size()),
        [&](tbb::blocked_range<size_t> r) {
#ifndef IPC_EARLIEST_TOI_USE_MUTEX
            double& earliest_toi = storage.local();
#endif
            for (size_t i = r.begin(); i < r.end(); i++) {
                double toi = std::numeric_limits<double>::infinity();
                bool are_colliding = candidates[i].ccd(
                    V0, V1, E, F, toi, /*tmax=*/earliest_toi, tolerance,
                    max_iterations);

                if (are_colliding) {
#ifdef IPC_EARLIEST_TOI_USE_MUTEX
                    std::lock_guard<std::mutex> lock(earliest_toi_mutex);
#endif
                    if (toi < earliest_toi) {
                        earliest_toi = toi;
                    }
                }
            }
        });

#ifndef IPC_EARLIEST_TOI_USE_MUTEX
    double earliest_toi = 1;
    for (const auto& local_earliest_toi : storage) {
        earliest_toi = std::min(earliest_toi, local_earliest_toi);
    }
#endif
    assert(earliest_toi >= 0 && earliest_toi <= 1.0);
    return earliest_toi;
}

///////////////////////////////////////////////////////////////////////////////

// NOTE: Actually distance squared
double compute_minimum_distance(
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V,
    const Constraints& constraint_set)
{
    assert(V.rows() == mesh.num_vertices());

    if (constraint_set.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    const Eigen::MatrixXi& E = mesh.edges();
    const Eigen::MatrixXi& F = mesh.faces();

    tbb::enumerable_thread_specific<double> storage(
        std::numeric_limits<double>::infinity());

    // Do a single block range over all constraint vectors
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, constraint_set.size()),
        [&](tbb::blocked_range<size_t> r) {
            double& local_min_dist = storage.local();

            for (size_t i = r.begin(); i < r.end(); i++) {
                const double dist = constraint_set[i].compute_distance(V, E, F);

                if (dist < local_min_dist) {
                    local_min_dist = dist;
                }
            }
        });

    double min_dist = std::numeric_limits<double>::infinity();
    for (const auto& local_min_dist : storage) {
        min_dist = std::min(min_dist, local_min_dist);
    }
    return min_dist;
}

///////////////////////////////////////////////////////////////////////////////

bool has_intersections(
    const CollisionMesh& mesh,
    const Eigen::MatrixXd& V,
    const BroadPhaseMethod& method)
{
    assert(V.rows() == mesh.num_vertices());
    const Eigen::MatrixXi& E = mesh.edges();
    const Eigen::MatrixXi& F = mesh.faces();

    double conservative_inflation_radius = 1e-2 * world_bbox_diagonal_length(V);

    // TODO: Expose the broad-phase method
    std::unique_ptr<BroadPhase> broad_phase =
        BroadPhase::make_broad_phase(method);
    broad_phase->can_vertices_collide = mesh.can_collide;

    broad_phase->build(V, E, F, conservative_inflation_radius);

    if (V.cols() == 2) { // Need to check segment-segment intersections in 2D
        std::vector<EdgeEdgeCandidate> ee_candidates;

        broad_phase->detect_edge_edge_candidates(ee_candidates);
        broad_phase->clear();

        // narrow-phase using igl
        igl::predicates::exactinit();
        for (const EdgeEdgeCandidate& ee_candidate : ee_candidates) {
            if (igl::predicates::segment_segment_intersect(
                    V.row(E(ee_candidate.edge0_index, 0)).head<2>(),
                    V.row(E(ee_candidate.edge0_index, 1)).head<2>(),
                    V.row(E(ee_candidate.edge1_index, 0)).head<2>(),
                    V.row(E(ee_candidate.edge1_index, 1)).head<2>())) {
                return true;
            }
        }
    } else { // Need to check segment-triangle intersections in 3D
        assert(V.cols() == 3);

        std::vector<EdgeFaceCandidate> ef_candidates;
        broad_phase->detect_edge_face_candidates(ef_candidates);
        broad_phase->clear();

        for (const EdgeFaceCandidate& ef_candidate : ef_candidates) {
            if (is_edge_intersecting_triangle(
                    V.row(E(ef_candidate.edge_index, 0)),
                    V.row(E(ef_candidate.edge_index, 1)),
                    V.row(F(ef_candidate.face_index, 0)),
                    V.row(F(ef_candidate.face_index, 1)),
                    V.row(F(ef_candidate.face_index, 2)))) {
                return true;
            }
        }
    }

    return false;
}
} // namespace ipc
