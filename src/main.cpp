#include <iostream>

#include <Teuchos_RCP.hpp>
#include "Teuchos_ParameterList.hpp"

#include "Epetra_Map.h"
#include "Epetra_Import.h"
#include "Epetra_Vector.h"
#include "Epetra_MultiVector.h"
#include "Epetra_CrsMatrix.h"
#include "Epetra_SerialDenseMatrix.h"

#ifdef HAVE_MPI
#include "Epetra_MpiComm.h"
#else
#include "Epetra_SerialComm.h"
#endif

#include "EpetraExt_CrsMatrixIn.h"
#include "EpetraExt_OperatorOut.h"
#include "EpetraExt_MultiVectorOut.h"

#include "Epetra_MultiVectorWrapper.hpp"
#include "Epetra_SerialDenseMatrixWrapper.hpp"
#include "Epetra_OperatorWrapper.hpp"
#include "LyapunovSolver.hpp"
#include "LyapunovMacros.hpp"
#include "SchurOperator.hpp"

#define TIMER_ON
#include "Timer.hpp"

// Parallel Projection Lanczos Lyapunov Solver
int main(int argc, char *argv[])
{
#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
#endif
#ifdef HAVE_MPI
    Teuchos::RCP<Epetra_MpiComm> Comm = Teuchos::rcp(new Epetra_MpiComm(MPI_COMM_WORLD));
#else
    Teuchos::RCP<Epetra_SerialComm> Comm = Teuchos::rcp(new Epetra_SerialComm());
#endif

    //Get process ID and total number of processes
    int MyPID = Comm->MyPID();
//     int NumProc = Comm->NumProc();

    Epetra_CrsMatrix *A_ptr, *B_ptr, *M_ptr, *SC_ptr;

    std::cout << "Loading matrices" << std::endl;

    CHECK_ZERO(EpetraExt::MatrixMarketFileToCrsMatrix("A.mtx", *Comm, A_ptr));
    CHECK_ZERO(EpetraExt::MatrixMarketFileToCrsMatrix("B.mtx", *Comm, B_ptr));
    CHECK_ZERO(EpetraExt::MatrixMarketFileToCrsMatrix("M.mtx", *Comm, M_ptr));

    Teuchos::RCP<Epetra_CrsMatrix> A = Teuchos::rcp(A_ptr);
    Teuchos::RCP<Epetra_CrsMatrix> B = Teuchos::rcp(B_ptr);
    Teuchos::RCP<Epetra_CrsMatrix> M = Teuchos::rcp(M_ptr);

    Epetra_BlockMap const &map = A->Map();

    Epetra_Vector diag_m(map);
    M->ExtractDiagonalCopy(diag_m);

    int *indices1 = new int[diag_m.MyLength()];
    int *indices2 = new int[diag_m.MyLength()];

    int num_indices1 = 0;
    int num_indices2 = 0;

    // Iterate over M looking for nonzero parts
    for (int i = 0; i < diag_m.MyLength(); i++)
    {
        if (abs(diag_m[i]) < 1e-15)
            indices1[num_indices1++] = map.GID(i);
        else
            indices2[num_indices2++] = map.GID(i);
    }

    Epetra_Map map1(-1, num_indices1, indices1, 0, *Comm);
    Epetra_Map map2(-1, num_indices2, indices2, 0, *Comm);

    delete[] indices1;
    delete[] indices2;

    Epetra_Map const &colMap = A->ColMap();

    Epetra_Vector diag_m_col(colMap);
    Epetra_Import colImport(colMap, map);
    diag_m_col.Import(diag_m, colImport, Insert);

    indices1 = new int[colMap.NumMyElements()];
    indices2 = new int[colMap.NumMyElements()];

    num_indices1 = 0;
    num_indices2 = 0;

    // Iterate over M looking for nonzero parts
    for (int i = 0; i < colMap.NumMyElements(); i++)
    {
        if (abs(diag_m_col[i]) < 1e-15)
            indices1[num_indices1++] = colMap.GID(i);
        else
            indices2[num_indices2++] = colMap.GID(i);
    }

    Epetra_Map colMap1(-1, num_indices1, indices1, 0, *Comm);
    Epetra_Map colMap2(-1, num_indices2, indices2, 0, *Comm);

    delete[] indices1;
    delete[] indices2;

    std::cout << "Computing Schur complement" << std::endl;

    Teuchos::RCP<SchurOperator> Schur = Teuchos::rcp(new SchurOperator(A, M));
    Schur->Compute();

    Epetra_Import import1(map1, map);
    Epetra_Import import2(map2, map);

    std::cout << "Splitting matrices" << std::endl;

    int MaxNumEntriesPerRow = A->MaxNumEntries();
    Teuchos::RCP<Epetra_CrsMatrix> A11 = Teuchos::rcp(
        new Epetra_CrsMatrix(Copy, map1, map1, MaxNumEntriesPerRow));
    CHECK_ZERO(A11->Import(*A, import1, Insert));
    CHECK_ZERO(A11->FillComplete(map1, map1));
    Teuchos::RCP<Epetra_CrsMatrix> A12 = Teuchos::rcp(
        new Epetra_CrsMatrix(Copy, map1, map2, MaxNumEntriesPerRow));
    CHECK_ZERO(A12->Import(*A, import1, Insert));
    CHECK_ZERO(A12->FillComplete(map2, map1));
    Teuchos::RCP<Epetra_CrsMatrix> A21 = Teuchos::rcp(
        new Epetra_CrsMatrix(Copy, map2, map1, MaxNumEntriesPerRow));
    CHECK_ZERO(A21->Import(*A, import2, Insert));
    CHECK_ZERO(A21->FillComplete(map1, map2));
    Teuchos::RCP<Epetra_CrsMatrix> A22 = Teuchos::rcp(
        new Epetra_CrsMatrix(Copy, map2, map2, MaxNumEntriesPerRow));
    CHECK_ZERO(A22->Import(*A, import2, Insert));
    CHECK_ZERO(A22->FillComplete(map2, map2));
    Teuchos::RCP<Epetra_CrsMatrix> B22 = Teuchos::rcp(
        new Epetra_CrsMatrix(Copy, map2, map2, MaxNumEntriesPerRow));
    CHECK_ZERO(B22->Import(*B, import2, Insert));
    CHECK_ZERO(B22->FillComplete(map2, map2));
    
    std::cout << "Creating solver" << std::endl;

    Teuchos::RCP<Epetra_Operator> Schur_operator = Schur;
    Teuchos::RCP<Epetra_Operator> B22_operator = B22;

    Lyapunov::Solver<Epetra_OperatorWrapper, Epetra_MultiVectorWrapper,
                     Epetra_SerialDenseMatrixWrapper> solver(
                         Schur_operator, B22_operator, B22_operator);

    Teuchos::RCP<Epetra_MultiVector> V = Teuchos::rcp(
        new Epetra_MultiVector(map2, 1000));
    Teuchos::RCP<Epetra_SerialDenseMatrix> T = Teuchos::rcp(
        new Epetra_SerialDenseMatrix(1000, 1000));

    Epetra_MultiVectorWrapper VW(V);
    Epetra_SerialDenseMatrixWrapper TW(T);

    Teuchos::ParameterList params;
    params.set("Maximum iterations", 1000);
    solver.set_parameters(params);

    std::cout << "Performing solve" << std::endl;

    solver.solve(VW, TW);

    if (!MyPID)
        SAVE_PROFILES("");

    EpetraExt::MultiVectorToMatrixMarketFile(
        "V.mtx", *VW);
    EpetraExt::MultiVectorToMatrixMarketFile(
        "T.mtx", *SerialDenseMatrixToMultiVector(View, *TW, V->Comm()));

#ifdef HAVE_MPI
    MPI_Finalize();
#endif
}
