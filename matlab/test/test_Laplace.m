function tests = test_Laplace
    tests = functiontests(localfunctions);
end

function test_Laplace_64(t)
    rng(4634);
    n = 64;
    A = -delsq(numgrid('S', sqrt(n)+2));
    M = spdiags(rand(n,1), 0, n, n);
    B = rand(n,1);

    [V, S, res, iter] = RAILSsolver(A,M,B);

    t.assertLessThan(iter, n-10);
    t.assertLessThan(res * norm(B'*B), 1E-2);
    t.assertLessThan(res, 1E-4);
    t.assertLessThan(norm(A*V*S*V'*M+M*V*S*V'*A'+B*B') / norm(B'*B), 1E-4);
end

function test_Laplace_256(t)
    rng(4634);
    n = 256;
    A = -delsq(numgrid('S', sqrt(n)+2));
    M = spdiags(rand(n,1), 0, n, n);
    B = rand(n,1);

    [V, S, res, iter] = RAILSsolver(A,M,B);

    t.assertLessThan(iter, n-10);
    t.assertLessThan(res * norm(B'*B), 1E-2);
    t.assertLessThan(res, 1E-4);
    t.assertLessThan(norm(A*V*S*V'*M+M*V*S*V'*A'+B*B') / norm(B'*B), 1E-4);
end

function test_Laplace_equivalence(t)
% Here we show that the Laplace problem is also a Lyapunov problem
    rng(4634);

    n = 1024;
    m = sqrt(n);

    A_lapl = -delsq(numgrid('S', m+2));
 
    e = ones(n, 1);
    A = spdiags([e, -2*e, e], -1:1, m, m);
    I = speye(m);
    A_kron = kron(A, I) + kron(I, A);
    t.assertEqual(A_lapl, A_kron);

    B = rand(m, 1);
    b = -reshape(B*B', n, 1);

    x_lapl = A_lapl \ b;

    [V, S, res, iter] = RAILSsolver(A, [], B);

    t.assertLessThan(res * norm(B'*B), 1E-2);
    t.assertLessThan(res, 1E-4);
    t.assertLessThan(norm(A*V*S*V'+V*S*V'*A'+B*B') / norm(B'*B), 1E-4);

    x_lyap = reshape(V*S*V', n, 1);
    t.assertLessThan(norm(x_lapl-x_lyap), 1E-4);
end