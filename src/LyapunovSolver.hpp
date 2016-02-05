#ifndef LYAPUNOVSOLVER_H
#define LYAPUNOVSOLVER_H

#include "LyapunovSolverDecl.hpp"
#include "SlicotWrapper.hpp"
#include "StlTools.hpp"

#include <string>
#include <locale>

#define TIMER_ON
#include "Timer.hpp"

namespace Lyapunov {

template<class Matrix, class MultiVector, class DenseMatrix>
Solver<Matrix, MultiVector, DenseMatrix>::Solver(Matrix const &A,
                                                 Matrix const &B,
                                                 Matrix const &M)
    :
    A_(A),
    B_(B),
    M_(M),
    max_iter_(1000),
    tol_(1e-3),
    expand_size_(3),
    lanczos_iterations_(10),
    restart_size_(-1),
    reduced_size_(-1),
    restart_iterations_(20),
    restart_tolerance_(tol_ * 1e-3),
    minimize_solution_space_(true)
{
}

template<class ParameterList, class Type>
Type get_parameter(ParameterList &params, std::string const &name, Type def)
{
    Type ret = params.get(name, def);

    std::locale loc;
    std::string str;

    // UPPER CASE
    str = name;
    for(std::string::iterator it = str.begin(); it != str.end(); ++it)
        *it = std::toupper(*it, loc);
    ret = params.get(str, ret);

    // lower case
    str = name;
    for(std::string::iterator it = str.begin(); it != str.end(); ++it)
        *it = std::tolower(*it, loc);
    ret = params.get(str, ret);

    // Title Case
    str = name;
    if(str.length() > 0)
        str[0] = std::toupper(str[0]);
    for(std::string::iterator it = str.begin() + 1; it != str.end(); ++it)
        if (!isalpha(*(it - 1)) && islower(*it))
            *it = std::toupper(*it, loc);
    ret = params.get(str, ret);

    return ret;
}

template<class Matrix, class MultiVector, class DenseMatrix>
template<class ParameterList>
int Solver<Matrix, MultiVector, DenseMatrix>::set_parameters(ParameterList &params)
{
    max_iter_ = get_parameter(params, "Maximum iterations", max_iter_);
    tol_ = get_parameter(params, "Tolerance", tol_);
    expand_size_ = get_parameter(params, "Expand size", expand_size_);
    lanczos_iterations_ = get_parameter(params, "Lanczos iterations", lanczos_iterations_);
    restart_size_ = get_parameter(params, "Restart size", restart_size_);
    reduced_size_ = get_parameter(params, "Reduced size", reduced_size_);
    restart_iterations_ = get_parameter(params, "Restart iterations", restart_iterations_);
    restart_tolerance_ = get_parameter(params, "Restart tolerance", tol_ * 1e-3);
    minimize_solution_space_ = get_parameter(params, "Minimize solution space",
                                             minimize_solution_space_);

    if (lanczos_iterations_ <= expand_size_)
    {
        std::cerr << "Amount of Lanczos iterations is smaller than "
                  << "the amount of vectors that are used to expand "
                  << "the space in every iteration" << std::endl;
        return 1;
    }

    return 0;
}

template<class Matrix, class MultiVector, class DenseMatrix>
class RestartOperator
{
public:
    MultiVector V;
    DenseMatrix T;

    RestartOperator(MultiVector &_V, DenseMatrix &_T): V(_V), T(_T) {}
    MultiVector operator *(MultiVector const &other) const
        {
            return V * (T * V.dot(other));
        }
};

template<class Matrix, class MultiVector, class DenseMatrix>
int Solver<Matrix, MultiVector, DenseMatrix>::solve(MultiVector &V, DenseMatrix &T)
{
    FUNCTION_TIMER("Solver");
    int n = V.M();

    int max_size = std::min(restart_size_ > 0 ? restart_size_ : 100, n);
    V.resize(max_size);

    V.resize(1);
    V.random();
    V.orthogonalize();

    MultiVector W = V;

    MultiVector AV(V, max_size);
    DenseMatrix VAV(max_size, max_size);
    AV.resize(0);

    MultiVector BV;
    DenseMatrix VBV(max_size, max_size);

    bool converged_previously = false;
    int previous_restart = 0;
    double r0 = B_.norm();

    for (int iter = 0; iter < max_iter_; iter++)
    {
        // Perform VAV = V'*A*V by doing VAV = [[VAV; W'*AV], V'*AW]

        // First compute AW and BW
        START_TIMER("Apply A");
        MultiVector AW = A_ * W;
        END_TIMER("Apply A");

        START_TIMER("Apply B");
        MultiVector BW = B_.transpose() * W;
        END_TIMER("Apply B");

        // Initialize BV which looks like B', not V
        if (!iter)
        {
            BV = MultiVector(BW, max_size);
            BV.resize(0);
        }

        START_TIMER("Compute VAV");
        // Now resize VAV. Resizing should not remove what was in VAV
        // previously
        int N_V = V.N();
        int N_AV = AV.N();
        int s = N_AV + W.N();
        VAV.resize(s, s);
        VBV.resize(s, s);

        // Now compute W'*AV and put it in VAV
        // We also compute V'*B*B'*V here for maximum efficiency
        // AV only exists after the first iteration
        if (N_AV > 0)
        {
            DenseMatrix WAV = W.dot(AV);
            DenseMatrix WBV = BW.dot(BV);
            int WAV_m = WAV.M();
            int WAV_n = WAV.N();
            for (int i = 0; i < WAV_m; i++)
                for (int j = 0; j < WAV_n; j++)
                {
                    VAV(i + N_AV, j) = WAV(i, j);
                    VBV(i + N_AV, j) = WBV(i, j);
                    VBV(j, i + N_AV) = WBV(i, j);
                }
        }

        // Now compute V'*AW and put it in VAV
        DenseMatrix VAW = V.dot(AW);
        int VAW_m = VAW.M();
        int VAW_n = VAW.N();
        for (int i = 0; i < VAW_m; i++)
            for (int j = 0; j < VAW_n; j++)
                VAV(i, j + N_AV) = VAW(i, j);

        // Compute the bottom-right block of VBV
        DenseMatrix WBW = BW.dot(BW);
        int WBW_m = WBW.M();
        int WBW_n = WBW.N();
        for (int i = 0; i < WBW_m; i++)
            for (int j = 0; j < WBW_n; j++)
                VBV(i + N_AV, j + N_AV) = WBW(i, j);

        // Now expand AV and BV
        AV.push_back(AW);
        BV.push_back(BW);

        END_TIMER("Compute VAV");

        dense_solve(VAV, VBV, T);

        int num_eigenvalues = lanczos_iterations_;
        DenseMatrix H(num_eigenvalues + 1, num_eigenvalues + 1);
        DenseMatrix eigenvalues(num_eigenvalues, 1);
        MultiVector eigenvectors;
        lanczos(AV, V, T, H, eigenvectors, eigenvalues, num_eigenvalues);

        double res = eigenvalues.norm_inf();

        std::cout << "Iteration " << iter+1
                  << ". Estimate Lanczos, absolute: " << res
                  << ", relative: " << std::abs(res) / r0 / r0 << std::endl;

        bool converged = std::abs(res) < tol_ * r0 * r0;
        if (converged || iter + 1 >= max_iter_ || V.N() >= n)
        {
            if (converged && minimize_solution_space_ && !converged_previously)
            {
                converged_previously = true;
            }
            else
            {
                std::cout << "The Lyapunov solver "
                          << (converged ? "converged" : "did not converge")
                          << " in " << iter+1
                          << " iterations with a final relative residual of "
                          << res / r0 / r0 << ". The size of the space used "
                          << "for the solution is " << V.N() << std::endl;
                break;
            }
        }

        // Restart the method with reduced_size_ vectors
        if ((restart_size_ > 0 && V.N() >= restart_size_)
            || (restart_iterations_ > 0 && iter - previous_restart >= restart_iterations_)
            || converged)
        {
            int reduced_size = std::min(reduced_size_ > 0 ? reduced_size_ : V.N(), V.N());
            if (converged)
                std::cout << "Method converged. Minimizing the solution "
                          << "space size";
            else if (restart_size_ > 0)
                std::cout << "Reached the maximum space size of " << restart_size_;
            else if (restart_iterations_ > 0)
                std::cout << restart_iterations_ << " iterations have passed";
            else
                std::cout << "No clue what happened";
            std::cout << ". Trying to restart with " << reduced_size
                      << " vectors" << std::endl;

            RestartOperator<Matrix, MultiVector, DenseMatrix> op(V, T);
            Matrix mat = Matrix::from_operator(op);
            DenseMatrix eigs;
            mat.eigs(V, eigs, reduced_size, restart_tolerance_);

            std::cout << "Restarted with " << V.N()
                      << " vectors" << std::endl;

            V.orthogonalize();
            W = V;

            VAV.resize(0, 0);
            AV.resize(0);

            VBV.resize(0, 0);
            BV.resize(0);

            previous_restart = iter;

            continue;
        }

        int expand_vectors = std::min(std::min(expand_size_, eigenvalues.M()),
                                      (restart_size_ > 0 ? restart_size_ : n) - V.N());

        // Allocate extra memory if needed. We do this only once per 100
        // vectors because it might be very expensive
        if (V.N() + expand_vectors > max_size)
        {
            START_TIMER("Resize spaces");
            max_size += 100;
            int previous_size = V.N();

            V.resize(max_size);
            V.resize(previous_size);

            AV.resize(max_size);
            AV.resize(previous_size);

            VAV.resize(max_size, max_size);
            VAV.resize(previous_size, previous_size);

            BV.resize(max_size);
            BV.resize(previous_size);

            VBV.resize(max_size, max_size);
            VBV.resize(previous_size, previous_size);
            END_TIMER("Resize spaces");
        }

        // Find the vectors belonging to the largest eigenvalues
        std::vector<int> indices;
        find_largest_eigenvalues(eigenvalues, indices, expand_vectors);

        for (int i = 0; i < expand_vectors; i++)
            V.push_back(eigenvectors.view(indices[i]));
        V.orthogonalize();


        W = V.view(N_V, N_V+expand_vectors-1);
    }

    return 0;
}

template<class Matrix, class MultiVector, class DenseMatrix>
int Solver<Matrix, MultiVector, DenseMatrix>::dense_solve(DenseMatrix const &A, DenseMatrix const &B, DenseMatrix &X)
{
    FUNCTION_TIMER("dense_solve");
    X = B.copy();
    DenseMatrix A_copy = A.copy();
    double scale = 1.0;
    int info = 0;
    int n = A.M();
    sb03md('C', 'X', 'N', 'T', n, A_copy, A_copy.LDA(), X, X.LDA(), &scale, &info);

    X.scale(-1.0);

    if (info)
        std::cerr << "Error: sb03md returned info = " << info << std::endl;

    return info;
}

template<class Matrix, class MultiVector, class DenseMatrix>
int Solver<Matrix, MultiVector, DenseMatrix>::lanczos(MultiVector const &AV, MultiVector const &V, DenseMatrix const &T, DenseMatrix &H, MultiVector &eigenvectors, DenseMatrix &eigenvalues, int max_iter)
{
    FUNCTION_TIMER("Lyapunov", "lanczos");
    START_TIMER("Lanczos", "Top");
    MultiVector Q(V, max_iter+1);
    Q.resize(1);
    Q.random();
    Q.view(0) /= Q.norm();

    H.scale(0.0);
    END_TIMER("Lanczos", "Top");

    double alpha = 0.0;
    double beta = 0.0;

    int iter = 0;
    for (int i = 0; i < max_iter; i++)
    {
        Q.resize(iter + 2);

        START_TIMER("Lanczos", "B apply");
        MultiVector Y = B_.transpose() * Q.view(iter);
        Q.view(iter+1) = B_ * Y;
        END_TIMER("Lanczos", "B apply");

        START_TIMER("Lanczos", "First part");
        DenseMatrix Z = V.dot(Q.view(iter));
        Z = T * Z;
        Q.view(iter+1) += AV * Z;
        END_TIMER("Lanczos", "First part");

        START_TIMER("Lanczos", "Second part");
        Z = AV.dot(Q.view(iter));
        Z = T * Z;
        Q.view(iter+1) += V * Z;
        END_TIMER("Lanczos", "Second part");

        START_TIMER("Lanczos", "alpha");
        alpha = Q.view(iter+1).dot(Q.view(iter))(0, 0);
        H(iter, iter) = alpha;
        END_TIMER("Lanczos", "alpha");

        START_TIMER("Lanczos", "update");
        Q.view(iter+1) -= alpha * Q.view(iter);
        if (iter > 0)
            Q.view(iter+1) -= beta * Q.view(iter-1);
        END_TIMER("Lanczos", "update");

        START_TIMER("Lanczos", "beta");
        beta = Q.view(iter+1).norm();
        END_TIMER("Lanczos", "beta");
        if (beta < 1e-14)
        {
            // Beta is zero, but the offdiagonal elements of H
            // might still be large, in which case we need those.
            // We can't continue because then we would divide by 0.
            iter++;
            break;
        }

        H(iter+1, iter) = beta;
        H(iter, iter+1) = beta;

        Q.view(iter+1) /= beta;

        iter++;
    }

    START_TIMER("Lanczos", "eigv");
    H.resize(iter, iter);
    Q.resize(iter);

    DenseMatrix v(iter, iter);
    H.eigs(v, eigenvalues);

    eigenvectors = Q * v;
    END_TIMER("Lanczos", "eigv");

    return 0;
}

}

#endif
