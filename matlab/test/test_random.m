function test_suite = test_random
    try
        test_functions = localfunctions();
        test_suite = functiontests(test_functions);
    catch
    end

    try
        initTestSuite;
    catch
    end
end

function seed()
    if ~exist('rng')
        rand('state', 4634);
    else
        rng(4634)
    end
end

function test_random_ev(t)
    seed;
    n = 64;
    A = sprand(n,n,10/n);
    M = speye(n);
    [B,~] = eigs(A,1);

    [V, S, res, iter] = RAILSsolver(A, M, B, 64);

    t.assertLessThan(iter, 10);
    t.assertLessThan(res * norm(B'*B), 1E-2);
    t.assertLessThan(res, 1E-4);
    t.assertLessThan(norm(A*V*S*V'*M'+M*V*S*V'*A'+B*B') / norm(B'*B), 1E-4);
end

function test_random_64(t)
    seed;
    n = 64;
    A = sprand(n,n,10/n);
    B = rand(n,1);
    M = spdiags(rand(n,1), 0, n, n);

    opts.restart_upon_convergence = false;
    [V, S, res] = RAILSsolver(A, M, B, opts);

    t.assertLessThan(res * norm(B'*B), 1E-2);
    t.assertLessThan(res, 1E-4);
    t.assertLessThan(norm(A*V*S*V'*M'+M*V*S*V'*A'+B*B') / norm(B'*B), 1E-4);
end