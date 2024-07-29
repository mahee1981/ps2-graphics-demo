#pragma once

class IDisposable
{
    protected:
    bool disposed;
    public:
    virtual void Dispose() = 0;
    IDisposable() : disposed(false) {}
};