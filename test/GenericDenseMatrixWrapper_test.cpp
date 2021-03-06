#include "gtest/gtest.h"

#include "TestHelpers.hpp"

#include "src/StlWrapper.hpp"
#include "src/StlTools.hpp"

#ifdef ENABLE_TRILINOS
#include "Epetra_TestableWrappers.hpp"
#endif

using namespace RAILS;

template <class DenseMatrixWrapper>
class GenericDenseMatrixWrapperTest: public testing::Test
{
protected:
    DenseMatrixWrapper A;
    DenseMatrixWrapper B;
    DenseMatrixWrapper C;
    DenseMatrixWrapper D;

    DenseMatrixWrapper a;
    DenseMatrixWrapper b;
    DenseMatrixWrapper c;

    GenericDenseMatrixWrapperTest()
        :
        A(2, 2),
        B(2, 2),
        C(2, 2),
        D(10, 10),
        a(2, 1),
        b(2, 1),
        c(2, 1)
        {}

    virtual ~GenericDenseMatrixWrapperTest() {}

    virtual void SetUp()
        {
        }

    virtual void TearDown()
        {
        }
};

#if GTEST_HAS_TYPED_TEST

using testing::Types;

#ifdef ENABLE_TRILINOS
typedef Types<StlWrapper, Epetra_SerialDenseMatrixWrapper> Implementations;
#else
typedef Types<StlWrapper> Implementations;
#endif

TYPED_TEST_CASE(GenericDenseMatrixWrapperTest, Implementations);

TYPED_TEST(GenericDenseMatrixWrapperTest, Eigs)
{
    this->A(0,0) = 1;
    this->A(0,1) = 3;
    this->A(1,0) = 3;
    this->A(1,1) = 4;

    this->A.eigs(this->B, this->C);

    EXPECT_NEAR((5.0 - sqrt(25.0 + 20.0)) / 2.0, this->C(0,0), 1e-6);
    EXPECT_NEAR((5.0 + sqrt(25.0 + 20.0)) / 2.0, this->C(1,0), 1e-6);
}

TYPED_TEST(GenericDenseMatrixWrapperTest, Eigs2)
{
    this->D = 0.0;
    this->D(0, 5) = 10.0;
    this->D(5, 0) = 10.0;
    this->D.resize(4, 4);
    this->D(0,0) = 1;
    this->D(0,1) = 3;
    this->D(1,0) = 3;
    this->D(1,1) = 4;

    this->D.eigs(this->B, this->C);

    std::vector<int> indices;
    find_largest_eigenvalues(this->C, indices, 4);

    EXPECT_NEAR((5.0 + sqrt(25.0 + 20.0)) / 2.0, this->C(indices[0],0), 1e-6);
    EXPECT_NEAR((5.0 - sqrt(25.0 + 20.0)) / 2.0, this->C(indices[1],0), 1e-6);
}

TYPED_TEST(GenericDenseMatrixWrapperTest, PutScalar)
{
    this->A = 2.0;

    for (int i = 0; i < this->A.M(); ++i)
        for (int j = 0; j < this->A.N(); ++j)
           this->B(i, j) = 2.0;

    EXPECT_VECTOR_EQ(this->B, this->A);
}

TYPED_TEST(GenericDenseMatrixWrapperTest, Scale)
{
    this->A = 0.0;
    this->D = 0.0;
    this->D(1, 1) = 1.0;
    this->D.resize(2, 2);
    this->D = 0.0;

    EXPECT_VECTOR_EQ(this->A, this->D);
}

TYPED_TEST(GenericDenseMatrixWrapperTest, Resize)
{
    int m = 80;
    int n = 100;
    this->A = 0.0;
    this->A(0,0) = 10.0;
    this->A.resize(m, n);
    EXPECT_EQ(m, this->A.M());
    EXPECT_EQ(m, this->A.LDA());
    EXPECT_EQ(n, this->A.N());
    EXPECT_EQ(10.0, this->A(0,0));
}

TYPED_TEST(GenericDenseMatrixWrapperTest, Resize2)
{
    // Check that we can make an empty wrapper
    TypeParam A;
    A.resize(10, 10);
}

TYPED_TEST(GenericDenseMatrixWrapperTest, View)
{
    this->A = 2.0;
    this->D = 1.0;

    // double *raw = &this->D(0, 0); // Doesn't work with Epetra
    this->D.resize(2, 2);

    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
        {
            ASSERT_EQ(2.0, this->A(i, j));
            ASSERT_EQ(1.0, this->D(i, j));
        }

    this->D.view() = this->A;

    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
        {
            ASSERT_EQ(2.0, this->D(i, j));
        }

    this->D.resize(10, 10);

    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
        {
            ASSERT_EQ(2.0, this->D(i, j));
            // ASSERT_EQ(2.0, raw[i+this->D.M()*j]);
        }
}

TYPED_TEST(GenericDenseMatrixWrapperTest, NormInf)
{
    this->A(0,0) = 1;
    this->A(0,1) = 3;
    this->A(1,0) = -3;
    this->A(1,1) = 4;

    double nrm = this->A.norm_inf();
    EXPECT_DOUBLE_EQ(7.0, nrm);
}

TYPED_TEST(GenericDenseMatrixWrapperTest, Transpose)
{
    this->A(0,0) = 1;
    this->A(0,1) = 2;
    this->A(1,0) = 3;
    this->A(1,1) = 4;

    this->a(0,0) = 1.2423;
    this->a(1,0) = -4.9693;

    this->b = this->A.transpose() * this->a;
    this->c = this->A * this->a;

    EXPECT_NEAR(this->a(0, 0) + 3.0 * this->a(1, 0), this->b(0, 0), 1e-14);
    EXPECT_NEAR(2.0 * this->a(0, 0) + 4.0 * this->a(1, 0), this->b(1, 0), 1e-14);

    EXPECT_NEAR(this->a(0, 0) + 2.0 * this->a(1, 0), this->c(0, 0), 1e-14);
    EXPECT_NEAR(3.0 * this->a(0, 0) + 4.0 * this->a(1, 0), this->c(1, 0), 1e-14);
}

TYPED_TEST(GenericDenseMatrixWrapperTest, Transpose2)
{
    EXPECT_EQ(2, this->a.M());
    EXPECT_EQ(1, this->a.N());
    EXPECT_EQ(1, this->a.transpose().M());
    EXPECT_EQ(2, this->a.transpose().N());
}

#endif
