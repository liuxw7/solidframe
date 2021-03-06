// solid/frame/service.hpp
//
// Copyright (c) 2013, 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/frame/common.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/schedulerbase.hpp"
#include "solid/utility/any.hpp"
#include "solid/utility/dynamictype.hpp"
#include <atomic>
#include <mutex>
#include <vector>

namespace solid {
struct Event;
namespace frame {

class ActorBase;

class Service;

template <class S = Service>
class ServiceShell;

struct UseServiceShell {
    Manager& rmanager;

    UseServiceShell()        = delete;
    UseServiceShell& operator=(const UseServiceShell&) = delete;
    UseServiceShell& operator=(UseServiceShell&&) = delete;

    UseServiceShell(UseServiceShell&& _uss)
        : rmanager(_uss.rmanager)
    {
    }
    UseServiceShell(const UseServiceShell& _uss)
        : rmanager(_uss.rmanager)
    {
    }

private:
    template <class S>
    friend class ServiceShell;

    explicit UseServiceShell(Manager& _rmanager)
        : rmanager(_rmanager)
    {
    }
};

class Service : NonCopyable {
    enum struct StatusE {
        Stopped,
        Running,
        Stopping,
    };

protected:
    explicit Service(
        UseServiceShell _force_shell, const bool _start = true);

    template <typename A>
    Service(
        UseServiceShell _force_shell, A&& _a, const bool _start = true);

public:
    Service(const Service&) = delete;
    Service(Service&&)      = delete;
    Service& operator=(const Service&) = delete;
    Service& operator=(Service&&) = delete;

    virtual ~Service();

    bool registered() const;

    void notifyAll(Event const& _e);

    template <class F>
    bool forEach(F& _rf);

    void stop(const bool _wait = true);

    Manager& manager();

    std::mutex& mutex(const ActorBase& _ract) const;

    ActorIdT id(const ActorBase& _ract) const;

    bool running() const;

    bool stopping() const;

    bool stopped() const;

    template <class A>
    A* any()
    {
        return any_.cast<A>();
    }

protected:
    std::mutex& mutex() const;

    void doStart();

    template <typename A>
    void doStart(A&& _a);

    template <typename A, typename F>
    void doStartWithAny(A&& _a, F&& _f);

    template <typename F>
    void doStartWithoutAny(F&& _f);

private:
    friend class Manager;
    friend class SchedulerBase;

    ActorIdT registerActor(ActorBase& _ract, ReactorBase& _rr, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr);
    //called by manager to set status

    bool statusSetStopping();
    void statusSetStopped();
    void statusSetRunning();

    size_t index() const;
    void   index(const size_t _idx);

private:
    Manager&             rm_;
    std::atomic<size_t>  idx_;
    std::atomic<StatusE> status_;
    Any<>                any_;
};

inline Service::Service(
    UseServiceShell _force_shell, const bool _start)
    : rm_(_force_shell.rmanager)
    , idx_(static_cast<size_t>(InvalidIndex()))
{
    rm_.registerService(*this, _start);
}

template <typename A>
inline Service::Service(
    UseServiceShell _force_shell, A&& _a, const bool _start)
    : rm_(_force_shell.rmanager)
    , idx_(static_cast<size_t>(InvalidIndex()))
    , any_(std::forward<A>(_a))
{
    rm_.registerService(*this, _start);
}

inline Service::~Service()
{
    stop(true);
    rm_.unregisterService(*this);
}

template <class F>
inline bool Service::forEach(F& _rf)
{
    return rm_.forEachServiceActor(*this, _rf);
}

inline Manager& Service::manager()
{
    return rm_;
}

inline bool Service::registered() const
{
    return idx_.load(/*std::memory_order_seq_cst*/) != InvalidIndex();
}

inline bool Service::running() const
{
    return status_.load(std::memory_order_relaxed) == StatusE::Running;
}

inline bool Service::stopping() const
{
    return status_.load(std::memory_order_relaxed) == StatusE::Stopping;
}

inline bool Service::stopped() const
{
    return status_.load(std::memory_order_relaxed) == StatusE::Stopped;
}

inline size_t Service::index() const
{
    return idx_.load();
}

inline void Service::index(const size_t _idx)
{
    idx_.store(_idx);
}

inline bool Service::statusSetStopping()
{
    StatusE expect{StatusE::Running};
    return status_.compare_exchange_strong(expect, StatusE::Stopping);
}

inline void Service::statusSetStopped()
{
    status_.store(StatusE::Stopped);
}

inline void Service::statusSetRunning()
{
    status_.store(StatusE::Running);
}

inline void Service::notifyAll(Event const& _revt)
{
    rm_.notifyAll(*this, _revt);
}

inline void Service::doStart()
{
    rm_.startService(
        *this, []() {}, []() {});
}

template <typename A>
inline void Service::doStart(A&& _a)
{
    Any<> a{std::forward<A>(_a)};
    rm_.startService(
        *this, [this, &a]() { any_ = std::move(a); }, []() {});
}

template <typename A, typename F>
inline void Service::doStartWithAny(A&& _a, F&& _f)
{
    Any<> a{std::forward<A>(_a)};
    rm_.startService(
        *this, [this, &a]() { any_ = std::move(a); }, std::forward<F>(_f));
}

template <typename F>
inline void Service::doStartWithoutAny(F&& _f)
{
    rm_.startService(
        *this, []() {}, std::forward<F>(_f));
}

inline void Service::stop(const bool _wait)
{
    rm_.stopService(*this, _wait);
}

inline std::mutex& Service::mutex(const ActorBase& _ract) const
{
    return rm_.mutex(_ract);
}

inline ActorIdT Service::id(const ActorBase& _ract) const
{
    return rm_.id(_ract);
}

inline std::mutex& Service::mutex() const
{
    return rm_.mutex(*this);
}

inline ActorIdT Service::registerActor(ActorBase& _ract, ReactorBase& _rr, ScheduleFunctionT& _rfct, ErrorConditionT& _rerr)
{
    return rm_.registerActor(*this, _ract, _rr, _rfct, _rerr);
}

//! A Shell class for every Service
/*!
 * This class is provided for defensive C++ programming.
 * Actors from a Service use reference to their service.
 * Situation: we have ServiceA: public Service. Actors from ServiceA use reference to ServiceA.
 * If we only call Service::stop() from within frame::Service::~Service, when ServiceA gets destroyed,
 * existing actors (at the moment we call Service::stop) might be still accessing ServiceA actor layer
 * which was destroyed.
 * That is why we've introduce the ServiceShell which will stand as an upper layer for all Service
 * instantiations which will call Service::stop on its destructor, so that when the lower layer Service
 * gets destroyed no actor will exist.
 * ServiceShell is final to prevent inheriting from it.
 * More over, we introduce the UseServiceShell stub to force all Service instantiations to happen through
 * a ServiceShell.
 */
template <class S>
class ServiceShell final : public S {
public:
    template <typename... Args>
    explicit ServiceShell(Manager& _rm, Args&&... _args)
        : S(UseServiceShell(_rm), std::forward<Args>(_args)...)
    {
    }

    ~ServiceShell()
    {
        Service::stop(true);
    }

    template <typename... Args>
    void start(Args&&... _args)
    {
        S::doStart(std::forward<Args>(_args)...);
    }
};

using ServiceT = ServiceShell<>;

} //namespace frame
} //namespace solid
