#ifndef __ECELL4_ODE_ODE_FACTORY2_HPP
#define __ECELL4_ODE_ODE_FACTORY2_HPP

#include <ecell4/core/SimulatorFactory.hpp>
#include <ecell4/core/extras.hpp>

#include "ODEWorld.hpp"
#include "ODESimulator.hpp"


namespace ecell4
{

namespace ode
{

class ODEFactory:
    public SimulatorFactory<ODEWorld, ODESimulator>
{
public:

    typedef SimulatorFactory<ODEWorld, ODESimulator> base_type;

public:

    ODEFactory(const ODESolverType solver_type = ROSENBROCK4_CONTROLLER,
               const Real dt = inf, const Real abs_tol = 0, const Real rel_tol = 0)
        : base_type(), solver_type_(solver_type),
        dt_(dt), abs_tol_(abs_tol), rel_tol_(rel_tol)
    {
        ; // do nothing
    }

    virtual ~ODEFactory()
    {
        ; // do nothing
    }

    virtual ODEWorld* create_world(const std::string filename) const
    {
        return new ODEWorld(filename);
    }

    virtual ODEWorld* create_world(
        const Real3& edge_lengths = Real3(1, 1, 1)) const
    {
        return new ODEWorld(edge_lengths);
    }

    virtual ODEWorld* create_world(const boost::shared_ptr<Model>& m) const
    {
        throw NotSupported("not supported.");
    }

    ODESimulator* create_simulator(
        const boost::shared_ptr<Model>& model,
        const boost::shared_ptr<world_type>& world) const
    {
        ODESimulator* sim = new ODESimulator(model, world, solver_type_);
        sim->set_dt(dt_);

        if (abs_tol_ > 0)
        {
            sim->set_absolute_tolerance(abs_tol_);
        }

        if (rel_tol_ > 0)
        {
            sim->set_relative_tolerance(rel_tol_);
        }
        return sim;
    }

    virtual ODESimulator* create_simulator(
        const boost::shared_ptr<world_type>& world) const
    {
        ODESimulator* sim = new ODESimulator(world, solver_type_);
        sim->set_dt(dt_);

        if (abs_tol_ > 0)
        {
            sim->set_absolute_tolerance(abs_tol_);
        }

        if (rel_tol_ > 0)
        {
            sim->set_relative_tolerance(rel_tol_);
        }
        return sim;
    }

    // ODESimulator* create_simulator(
    //     const boost::shared_ptr<NetworkModel>& model,
    //     const boost::shared_ptr<world_type>& world) const
    // {
    //     ODESimulator* sim = new ODESimulator(model, world, solver_type_);
    //     sim->set_dt(dt_);
    //     return sim;
    // }

    ODESimulator* create_simulator(
        const boost::shared_ptr<ODENetworkModel>& model,
        const boost::shared_ptr<world_type>& world) const
    {
        ODESimulator* sim = new ODESimulator(model, world, solver_type_);
        sim->set_dt(dt_);

        if (abs_tol_ > 0)
        {
            sim->set_absolute_tolerance(abs_tol_);
        }

        if (rel_tol_ > 0)
        {
            sim->set_relative_tolerance(rel_tol_);
        }
        return sim;
    }

protected:

    ODESolverType solver_type_;
    Real dt_, abs_tol_, rel_tol_;
};

} // ode

} // ecell4

#endif /* __ECELL4_ODE_ODE_FACTORY2_HPP */
