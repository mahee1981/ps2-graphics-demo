#ifndef IDISPOSABLE_HPP
#define IDISPOSABLE_HPP


class IDisposable
{
    protected:
    bool disposed;
    public:
    virtual void Dispose() = 0;
    IDisposable() : disposed(false) {}
};

#endif