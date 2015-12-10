#ifndef EPETRAWRAPPER_H
#define EPETRAWRAPPER_H

#include <Teuchos_RCP.hpp>
#include <Teuchos_LAPACK.hpp>

#include <Epetra_SerialDenseMatrix.h>
#include <Epetra_MultiVector.h>
#include <Epetra_CrsMatrix.h>
#include <Epetra_LocalMap.h>

#define TIMER_ON
#include "Timer.hpp"

// Helper functions
Teuchos::RCP<Epetra_SerialDenseMatrix> MultiVectorToSerialDenseMatrix(
    Epetra_DataAccess CV, Epetra_MultiVector const &src);
Teuchos::RCP<Epetra_MultiVector> SerialDenseMatrixToMultiVector(
    Epetra_DataAccess CV, Epetra_SerialDenseMatrix const &src,
    Epetra_Comm const &comm);

template<class WrapperType>
class EpetraWrapper
{
    Teuchos::RCP<WrapperType> ptr_;

    Teuchos::RCP<WrapperType> ptr_allocated_;

    // Capacity of a multivector
    int capacity_;

    // Size of a multivector (amount of vectors)
    int size_;

    // Amount of vectors that are already orthogonal
    int orthogonalized_;

    // Vector is a view
    bool is_view_;
public:
    EpetraWrapper()
        :
        ptr_(Teuchos::null),
        ptr_allocated_(Teuchos::null),
        capacity_(-1),
        size_(-1),
        orthogonalized_(0),
        is_view_(false)
        {}

    EpetraWrapper(Teuchos::RCP<WrapperType> ptr)
        :
        EpetraWrapper()
        {
            FUNCTION_TIMER("EpetraWrapper", "constructor 1");
            ptr_ = ptr;
        }

    EpetraWrapper(EpetraWrapper const &other)
        :
        EpetraWrapper()
        {
            FUNCTION_TIMER("EpetraWrapper", "constructor 2");
            if (!other.ptr_.is_null())
                ptr_ = Teuchos::rcp(new WrapperType(*other.ptr_));
            orthogonalized_ = other.orthogonalized_;
        }

    EpetraWrapper(EpetraWrapper const &other, int n)
        :
        EpetraWrapper()
        {
            FUNCTION_TIMER("EpetraWrapper", "constructor 3");
            size_ = n;
            capacity_ = n;
            if (!other.ptr_.is_null())
                ptr_ = Teuchos::rcp(new WrapperType(other.ptr_->Map(), n));
        }

    EpetraWrapper(int m, int n)
        :
        EpetraWrapper()
        {
            FUNCTION_TIMER("EpetraWrapper", "constructor 4");
            size_ = n;
            capacity_ = n;
            ptr_ = Teuchos::rcp(new WrapperType(m, n));
        }

    virtual ~EpetraWrapper() {}

    EpetraWrapper &operator =(EpetraWrapper &other);
    EpetraWrapper &operator =(EpetraWrapper const &other);

    EpetraWrapper &operator *=(double other)
        {
            FUNCTION_TIMER("EpetraWrapper", "*=");
            ptr_->Scale(other);
            return *this;
        }
    EpetraWrapper &operator /=(double other)
        {
            FUNCTION_TIMER("EpetraWrapper", "/=");
            ptr_->Scale(1.0 / other);
            return *this;
        }

    EpetraWrapper &operator -=(EpetraWrapper const &other)
        {
            FUNCTION_TIMER("EpetraWrapper", "-=");
            ptr_->Update(-1.0, *other, 1.0);
            return *this;
        }
    EpetraWrapper &operator +=(EpetraWrapper const &other)
        {
            FUNCTION_TIMER("EpetraWrapper", "+=");
            ptr_->Update(1.0, *other, 1.0);
            return *this;
        }

    EpetraWrapper operator *(EpetraWrapper const &other) const
        {
            FUNCTION_TIMER("EpetraWrapper", "*");
            EpetraWrapper e(*this);
            e *= other;
            return e;
        }

    EpetraWrapper operator +(EpetraWrapper const &other) const
        {
            FUNCTION_TIMER("EpetraWrapper", "+");
            EpetraWrapper e(*this);
            e += other;
            return e;
        }

    operator double*() const
        {
            FUNCTION_TIMER("EpetraWrapper", "double*");
            return ptr_->A();
        }

    WrapperType &operator *()
        {
            FUNCTION_TIMER("EpetraWrapper", "*");
            return *ptr_;
        }

    WrapperType const &operator *() const
        {
            FUNCTION_TIMER("EpetraWrapper", "* 2");
            return *ptr_;
        }

    double &operator ()(int m, int n = 0)
        {
            FUNCTION_TIMER("EpetraWrapper", "()");
            return (*ptr_)(m, n);
        }

    double const &operator ()(int m, int n = 0) const
        {
            FUNCTION_TIMER("EpetraWrapper", "() 2");
            return (*ptr_)(m, n);
        }

    void scale(double factor)
        {
            FUNCTION_TIMER("EpetraWrapper", "scale");
            ptr_->Scale(factor);
        }

    void set(double factor)
        {
            FUNCTION_TIMER("EpetraWrapper", "set");
            ptr_->PutScalar(factor);
        }

    void resize(int m)
        {
            FUNCTION_TIMER("EpetraWrapper", "resize");
            // Check if ptr_allocated_ is set
            if (ptr_allocated_.is_null())
                ptr_allocated_ = ptr_;

            // Set the capacity if it was not set
            if (capacity_ == -1)
                capacity_ = num_vectors();

            // Allocate more memory if needed, and copy the old vector
            if (capacity_ < m)
            {
                Teuchos::RCP<WrapperType> new_ptr = Teuchos::rcp(
                    new WrapperType(ptr_allocated_->Map(), m));
                if (!ptr_.is_null())
                    ptr_->ExtractCopy(new_ptr->Values(), new_ptr->MyLength());
                ptr_ = new_ptr;
                ptr_allocated_ = new_ptr;
                capacity_ = m;
            }
            else if (!m)
            {
                ptr_ = Teuchos::null;
            }
            else
            {
                // Now only view a part of ptr_allocated_
                ptr_ = Teuchos::rcp(new WrapperType(View, *ptr_allocated_, 0, m));
            }
            size_ = m;
        }

    void resize(int m, int n)
        {
            FUNCTION_TIMER("EpetraWrapper", "resize 2");
            ptr_->Reshape(m, n);
        }

    double norm(int n = 0)
        {
            FUNCTION_TIMER("EpetraWrapper", "norm");
            if (num_vectors() == 1)
            {
                double out;
                ptr_->Norm2(&out);
                return out;
            }
            return view(n).norm(n);
        }

    double norm_inf(int n = 0)
        {
            FUNCTION_TIMER("EpetraWrapper", "norm inf");
            return ptr_->NormInf();
        }

    double norm_frobenius(int n = 0)
        {
            FUNCTION_TIMER("EpetraWrapper", "norm frob");
            return ptr_->NormFrobenius();
        }

    void orthogonalize()
        {
            FUNCTION_TIMER("EpetraWrapper", "orthogonalize");
            for (int i = orthogonalized_; i < num_vectors(); i++)
            {
                EpetraWrapper v = view(i);
                v /= v.norm();
                if (i)
                {
                    EpetraWrapper V = view(0, i-1);
                    for (int k = 0; k < 2; k++)
                        v -= V.apply(V.dot(v));
                }
                v /= v.norm();
            }
            orthogonalized_ = num_vectors();
        }

    EpetraWrapper view(int m, int n = 0)
        {
            FUNCTION_TIMER("EpetraWrapper", "view");
            EpetraWrapper out;
            int num = n ? n-m+1 : 1;
            out.ptr_ = Teuchos::rcp(new WrapperType(View, *ptr_, m, num));
            out.is_view_ = true;
            return out;
        }

    EpetraWrapper copy(int m = 0, int n = 0) const
        {
            FUNCTION_TIMER("EpetraWrapper", "copy");
            EpetraWrapper<WrapperType> out(*this);
            return out;
        }

    void push_back(EpetraWrapper const &other, int m = -1)
        {
            FUNCTION_TIMER("EpetraWrapper", "push_back");
            int n = num_vectors();
            if (m == -1)
                m = other.num_vectors();
            resize(m + n);
            memcpy((*ptr_)[n], other.ptr_->Values(), sizeof(double)*m*ptr_->MyLength());
        }

    int M() const
        {
            FUNCTION_TIMER("EpetraWrapper", "M");
            return ptr_->M();
        }

    int N() const
        {
            FUNCTION_TIMER("EpetraWrapper", "N");
            return ptr_->N();
        }

    int GlobalLength() const
        {
            return ptr_->Map().NumGlobalPoints();
        }

    EpetraWrapper<Epetra_SerialDenseMatrix> dot(EpetraWrapper const &other) const
        {
            FUNCTION_TIMER("EpetraWrapper", "dot");

            START_TIMER("EpetraWrapper", "dot copy");
            Teuchos::RCP<Epetra_SerialDenseMatrix> mat = Teuchos::rcp(
                new Epetra_SerialDenseMatrix(num_vectors(), other.num_vectors()));
            Teuchos::RCP<Epetra_MultiVector> mv = SerialDenseMatrixToMultiVector(
                View, *mat, ptr_->Comm());
            END_TIMER("EpetraWrapper", "dot copy");

            START_TIMER("EpetraWrapper", "dot multiply");
            mv->Multiply('T', 'N', 1.0, *ptr_, *other.ptr_, 0.0);
            END_TIMER("EpetraWrapper", "dot multiply");

            START_TIMER("EpetraWrapper", "dot copy 2");
            EpetraWrapper<Epetra_SerialDenseMatrix> out(mat);
            END_TIMER("EpetraWrapper", "dot copy 2");
            return out;
        }

    int num_vectors() const
        {
            FUNCTION_TIMER("EpetraWrapper", "num_vectors");
            return (size_ ? ptr_->NumVectors() : size_);
        }

    EpetraWrapper apply(EpetraWrapper const &other) const;

    template<class WrapperType2>
    EpetraWrapper<Epetra_MultiVector> apply(EpetraWrapper<WrapperType2> const &other) const;


    void eigs(EpetraWrapper &v, EpetraWrapper &d) const
        {
            FUNCTION_TIMER("EpetraWrapper", "eigs");
            int m = M();
            v = copy();

            // Put the diagonal in d
            d.resize(m, 1);
            for (int i = 0; i < m; i++)
                d(i, 0) = v(i, i);

            // Put the offdiagonal in e
            Epetra_SerialDenseMatrix e(m-1, 1);
            for (int i = 0; i < m-1; i++)
                e(i, 0) = v(i+1, i);

            Epetra_SerialDenseMatrix work(std::max(1,2*m-2), 1);

            int info;
            Teuchos::LAPACK<int, double> lapack;
            lapack.STEQR('I', m, (*d).A(),
                          e.A(), (*v).A(),
                          m, work.A(), &info);

            if (info)
                std::cerr << "Eigenvalues info = " << info << std::endl;
        }

    void random()
        {
            FUNCTION_TIMER("EpetraWrapper", "random");
            ptr_->Random();
        }
};

template<class WrapperType>
EpetraWrapper<WrapperType> operator *(double d, EpetraWrapper<WrapperType> const &other)
{
    FUNCTION_TIMER("EpetraWrapper", "friend *");
    EpetraWrapper<WrapperType> e(other);
    e *= d;
    return e;
}

#endif
