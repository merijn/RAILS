#ifndef SCALARWRAPPER_H
#define SCALARWRAPPER_H

namespace RAILS
{

class ScalarWrapper
{
    double *s_;
    bool is_view_;
    int size_;
    int capacity_;
public:
    ScalarWrapper()
        :
        s_(new double[1]),
        is_view_(false) ,
        size_(1),
        capacity_(1)
        {}

    ScalarWrapper(double s)
        :
        s_(new double[1]),
        is_view_(false),
        size_(1),
        capacity_(1)
        {
            *s_ = s;
        }

    ScalarWrapper(ScalarWrapper const &s)
        :
        s_(),
        is_view_(false),
        size_(s.size_),
        capacity_(size_)
        {
            s_ = new double[size_];
            for (int i = 0; i < size_; i++)
                s_[i] = s.s_[i];
        }

    ScalarWrapper(ScalarWrapper const &s, int n)
        :
        s_(),
        is_view_(false),
        size_(n ? n : s.size_),
        capacity_(size_)
        {
            s_ = new double[size_];
        }

    template <class Operator>
    static ScalarWrapper from_operator(Operator &op)
        {
            return ScalarWrapper();
        }

    virtual ~ScalarWrapper()
        {
            if (!is_view_)
                delete[] s_;
        }

    ScalarWrapper &operator =(ScalarWrapper other)
        {
            if (!is_view_)
            {
                std::swap(s_, other.s_);
                std::swap(is_view_, other.is_view_);
                std::swap(size_, other.size_);
                std::swap(capacity_, other.capacity_);
                return *this;
            }

            memcpy(s_, other.s_, size_*sizeof(double));
            return *this;
        }

    ScalarWrapper operator *=(ScalarWrapper const &other) {*s_ *= *other.s_; return *this;}
    ScalarWrapper operator /=(ScalarWrapper const &other) {*s_ /= *other.s_; return *this;}
    ScalarWrapper operator -=(ScalarWrapper const &other) {*s_ -= *other.s_; return *this;}
    ScalarWrapper operator +=(ScalarWrapper const &other) {*s_ += *other.s_; return *this;}

    ScalarWrapper operator *(ScalarWrapper const &other) const {return *s_ * *other.s_;}
    ScalarWrapper operator +(ScalarWrapper const &other) const {return *s_ + *other.s_;}

    operator double() const {return *s_;};
    operator double*() {return s_;};
    operator const double*() const {return s_;};

    double &operator ()(int m, int n = 0) {return s_[m];}
    double const &operator ()(int m, int n = 0) const {return s_[m];}

    void resize(int m, int n = 0)
        {
            if (capacity_ < m)
            {
                double *s = new double[m];
                memcpy(s, s_, capacity_ * sizeof(double));
                delete[] s_;
                s_ = s;
                capacity_ = m;
            }
            size_ = m;
        }

    double norm() const {return *s_;}
    double norm_inf() const {return *s_;}
    void orthogonalize() {*s_ = 1;}

    ScalarWrapper transpose() const
        {
            return *this;
        }

    ScalarWrapper view(int m = 0, int n = 0)
        {
            ScalarWrapper s;
            delete[] s.s_;
            s.s_ = s_ + m;
            s.is_view_ = true;
            if (n > 0)
                s.size_ = n - m;
            return s;
        }
    ScalarWrapper copy() const
        {
            return *this;
        }

    void push_back(ScalarWrapper s, int n = 0)
        {
            resize(size_+1);
            s_[size_-1] = s;
        }

    int M() const {return 1;}
    int N() const {return size_;}
    int LDA() const {return 1;}
    ScalarWrapper dot(ScalarWrapper const &other) const {return *this * other;}
    void eigs(ScalarWrapper &v, ScalarWrapper &d, int num = 1, double tol = 1.0) const
        {
            *v.s_ = 1;
            *d.s_ = *s_;
        }

    void random() {*s_ = 0.2462561245;}
};

}

#endif
