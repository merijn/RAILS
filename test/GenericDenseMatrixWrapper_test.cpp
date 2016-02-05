#include "gtest/gtest.h"

#include "src/StlWrapper.hpp"
#include "src/StlTools.hpp"

#include "Epetra_TestableWrappers.hpp"

#define EXPECT_VECTOR_EQ(a, b) {                \
    int m = (a).M();                            \
    int n = (a).N();                            \
    for (int i = 0; i < m; i++)                 \
      for (int j = 0; j < n; j++)               \
        EXPECT_DOUBLE_EQ((a)(i,j), (b)(i,j));   \
    }

template <class DenseMatrixWrapper>
class GenericDenseMatrixWrapperTest: public testing::Test
{
protected:
    DenseMatrixWrapper A;
    DenseMatrixWrapper B;
    DenseMatrixWrapper C;
    DenseMatrixWrapper D;

    GenericDenseMatrixWrapperTest()
        :
        A(2, 2),
        B(2, 2),
        C(2, 2),
        D(10, 10)
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

typedef Types<StlWrapper, Epetra_SerialDenseMatrixWrapper> Implementations;

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
    this->D.scale(0.0);
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

TYPED_TEST(GenericDenseMatrixWrapperTest, Scale)
{
    this->A.scale(0.0);
    this->D.scale(0.0);
    this->D(1, 1) = 1.0;
    this->D.resize(2, 2);
    this->D.scale(0.0);

    EXPECT_VECTOR_EQ(this->A, this->D);
}

#endif
