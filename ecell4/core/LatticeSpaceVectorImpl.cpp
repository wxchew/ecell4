#include "Context.hpp"
#include "MolecularType.hpp"
#include "VacantType.hpp"
#include "StructureType.hpp"
#include "InterfaceType.hpp"
#include "LatticeSpaceVectorImpl.hpp"

namespace ecell4 {

LatticeSpaceVectorImpl::LatticeSpaceVectorImpl(
    const Real3& edge_lengths, const Real& voxel_radius,
    const bool is_periodic) :
    base_type(edge_lengths, voxel_radius, is_periodic), is_periodic_(is_periodic)
{
    vacant_ = &(VacantType::getInstance());
    std::stringstream ss;
    ss << voxel_radius_;
    border_ = new MolecularType(Species("Border", ss.str(), "0"));
    periodic_ = new MolecularType(Species("Periodic", ss.str(), "0"));

    initialize_voxels(is_periodic_);
}

LatticeSpaceVectorImpl::~LatticeSpaceVectorImpl()
{
    delete border_;
    delete periodic_;
}

void LatticeSpaceVectorImpl::initialize_voxels(const bool is_periodic)
{
    const coordinate_type voxel_size(
        col_size_ * row_size_ * layer_size_);
    // std::cout << "voxel_size = " << voxel_size << std::endl;

    voxel_pools_.clear();
    molecule_pools_.clear();
    voxels_.clear();
    voxels_.reserve(voxel_size);
    for (coordinate_type coord(0); coord < voxel_size; ++coord)
    {
        if (!is_inside(coord))
        {
            if (is_periodic)
            {
                voxels_.push_back(periodic_);
            }
            else
            {
                voxels_.push_back(border_);
            }
        }
        else
        {
            voxels_.push_back(vacant_);
        }
    }
}

Integer LatticeSpaceVectorImpl::num_species() const
{
    return voxel_pools_.size() + molecule_pools_.size();
}

Integer LatticeSpaceVectorImpl::num_molecules(const Species& sp) const
{
    Integer count(0);
    SpeciesExpressionMatcher sexp(sp);

    for (voxel_pool_map_type::const_iterator itr(voxel_pools_.begin());
         itr != voxel_pools_.end(); ++itr)
    {
        const Integer cnt(sexp.count((*itr).first));
        if (cnt > 0)
        {
            const boost::shared_ptr<VoxelPool>& mt((*itr).second);
            count += count_voxels(mt) * cnt;
        }
    }

    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        const Integer cnt(sexp.count((*itr).first));
        if (cnt > 0)
        {
            const boost::shared_ptr<MoleculePool>& mt((*itr).second);
            count += mt->size() * cnt;
        }
    }
    return count;
}

bool LatticeSpaceVectorImpl::has_voxel(const ParticleID& pid) const
{
    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        const boost::shared_ptr<MoleculePool>& mt((*itr).second);
        if (mt->find(pid) != mt->end())
        {
            return true;
        }
    }
    return false;
}

std::pair<ParticleID, Voxel>
LatticeSpaceVectorImpl::get_voxel(const ParticleID& pid) const
{
    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        const boost::shared_ptr<MoleculePool>& mt((*itr).second);
        MoleculePool::container_type::const_iterator j(mt->find(pid));
        if (j != mt->end())
        {
            const std::string loc((mt->location()->is_vacant())
                ? "" : mt->location()->species().serial());
            return std::make_pair(pid,
                Voxel((*itr).first, (*j).coordinate, mt->radius(), mt->D(), loc));
        }
    }
    throw NotFound("voxel not found.");
}

std::pair<ParticleID, Voxel>
LatticeSpaceVectorImpl::get_voxel(const coordinate_type& coord) const
{
    const VoxelPool* mt(voxels_[coord]);
    const std::string loc((mt->location()->is_vacant())
        ? "" : mt->location()->species().serial());
    return std::make_pair(
        mt->get_particle_id(coord),
        Voxel(mt->species(), coord, mt->radius(), mt->D(), loc));
}

bool LatticeSpaceVectorImpl::update_structure(const Particle& p)
{
    //XXX: Particle does not have a location.
    Voxel v(p.species(), position2coordinate(p.position()), p.radius(), p.D());
    return update_voxel(ParticleID(), v);
}

/*
 * original methods
 */

std::vector<Species> LatticeSpaceVectorImpl::list_species() const
{
    std::vector<Species> keys;
    for (voxel_pool_map_type::const_iterator itr(voxel_pools_.begin());
         itr != voxel_pools_.end(); ++itr)
    {
        keys.push_back((*itr).first);
    }

    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        keys.push_back((*itr).first);
    }
    return keys;
}

const Species& LatticeSpaceVectorImpl::find_species(std::string name) const
{
    for (voxel_pool_map_type::const_iterator itr(voxel_pools_.begin());
         itr != voxel_pools_.end(); ++itr)
    {
        if ((*itr).first.serial() == name)
        {
            return (*itr).first;
        }
    }

    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        if ((*itr).first.serial() == name)
        {
            return (*itr).first;
        }
    }
    throw NotFound(name);
}

std::vector<LatticeSpaceVectorImpl::coordinate_type>
    LatticeSpaceVectorImpl::list_coords_exact(const Species& sp) const
{
    std::vector<coordinate_type> retval;

    molecule_pool_map_type::const_iterator itr(molecule_pools_.find(sp));
    if (itr == molecule_pools_.end())
    {
        return retval;
    }

    const boost::shared_ptr<MoleculePool>& mt((*itr).second);

    for (MoleculePool::const_iterator itr(mt->begin()); itr != mt->end(); ++itr)
    {
        retval.push_back((*itr).coordinate);
    }
    return retval;
}

std::vector<LatticeSpaceVectorImpl::coordinate_type> LatticeSpaceVectorImpl::list_coords(const Species& sp) const
{
    std::vector<coordinate_type> retval;
    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        if (!spmatch(sp, (*itr).first))
        {
            continue;
        }

        const boost::shared_ptr<MoleculePool>& mt((*itr).second);

        for (MoleculePool::const_iterator vitr(mt->begin());
             vitr != mt->end(); ++vitr)
        {
            retval.push_back((*vitr).coordinate);
        }
    }
    return retval;
}

std::vector<std::pair<ParticleID, Voxel> >
LatticeSpaceVectorImpl::list_voxels() const
{
    std::vector<std::pair<ParticleID, Voxel> > retval;

    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        const boost::shared_ptr<MoleculePool>& mt((*itr).second);

        const std::string loc((mt->location()->is_vacant())
            ? "" : mt->location()->species().serial());
        const Species& sp(mt->species());

        for (MoleculePool::const_iterator i(mt->begin());
            i != mt->end(); ++i)
        {
            retval.push_back(std::make_pair(
                (*i).pid,
                Voxel(sp, (*i).coordinate, mt->radius(), mt->D(), loc)));
        }
    }

    for (voxel_pool_map_type::const_iterator itr(voxel_pools_.begin());
         itr != voxel_pools_.end(); ++itr)
    {
        const boost::shared_ptr<VoxelPool>& mt((*itr).second);

        const std::string loc((mt->location()->is_vacant())
            ? "" : mt->location()->species().serial());
        const Species& sp(mt->species());

        for (voxel_container::const_iterator i(voxels_.begin());
             i != voxels_.end(); ++i)
        {
            if (*i != mt.get())
            {
                continue;
            }

            const coordinate_type
                coord(std::distance(voxels_.begin(), i));
            retval.push_back(std::make_pair(
                ParticleID(),
                Voxel(sp, coord, mt->radius(), mt->D(), loc)));
        }
    }
    return retval;
}

std::vector<std::pair<ParticleID, Voxel> >
LatticeSpaceVectorImpl::list_voxels_exact(const Species& sp) const
{
    std::vector<std::pair<ParticleID, Voxel> > retval;

    {
        voxel_pool_map_type::const_iterator itr(voxel_pools_.find(sp));
        if (itr != voxel_pools_.end())
        {
            const boost::shared_ptr<VoxelPool>& mt((*itr).second);
            const std::string loc((mt->location()->is_vacant())
                ? "" : mt->location()->species().serial());
            for (voxel_container::const_iterator i(voxels_.begin());
                 i != voxels_.end(); ++i)
            {
                if (*i != mt.get())
                {
                    continue;
                }

                const coordinate_type
                    coord(std::distance(voxels_.begin(), i));
                retval.push_back(std::make_pair(
                    ParticleID(),
                    Voxel(sp, coord, mt->radius(), mt->D(), loc)));
            }
            return retval;
        }
    }

    {
        molecule_pool_map_type::const_iterator itr(molecule_pools_.find(sp));
        if (itr != molecule_pools_.end())
        {
            const boost::shared_ptr<MoleculePool>& mt((*itr).second);
            const std::string loc((mt->location()->is_vacant())
                ? "" : mt->location()->species().serial());
            for (MoleculePool::const_iterator i(mt->begin());
                 i != mt->end(); ++i)
            {
                retval.push_back(std::make_pair(
                    (*i).pid,
                    Voxel(sp, (*i).coordinate, mt->radius(), mt->D(), loc)));
            }
            return retval;
        }
    }
    return retval; // an empty vector
}

std::vector<std::pair<ParticleID, Voxel> >
LatticeSpaceVectorImpl::list_voxels(const Species& sp) const
{
    std::vector<std::pair<ParticleID, Voxel> > retval;
    SpeciesExpressionMatcher sexp(sp);

    for (voxel_pool_map_type::const_iterator itr(voxel_pools_.begin());
         itr != voxel_pools_.end(); ++itr)
    {
        if (!sexp.match((*itr).first))
        {
            continue;
        }

        const boost::shared_ptr<VoxelPool>& mt((*itr).second);
        const std::string loc((mt->location()->is_vacant())
            ? "" : mt->location()->species().serial());
        for (voxel_container::const_iterator i(voxels_.begin());
             i != voxels_.end(); ++i)
        {
            if (*i != mt.get())
            {
                continue;
            }

            const coordinate_type
                coord(std::distance(voxels_.begin(), i));
            retval.push_back(std::make_pair(
                ParticleID(),
                Voxel(sp, coord, mt->radius(), mt->D(), loc)));
        }
    }

    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        if (!sexp.match((*itr).first))
        {
            continue;
        }

        const boost::shared_ptr<MoleculePool>& mt((*itr).second);
        const std::string loc((mt->location()->is_vacant())
            ? "" : mt->location()->species().serial());
        for (MoleculePool::const_iterator i(mt->begin());
            i != mt->end(); ++i)
        {
            retval.push_back(std::make_pair(
                (*i).pid,
                Voxel(sp, (*i).coordinate, mt->radius(), mt->D(), loc)));
        }
    }

    return retval;
}

VoxelPool* LatticeSpaceVectorImpl::get_voxel_pool(const Voxel& v)
{
    const Species& sp(v.species());

    {
        voxel_pool_map_type::iterator itr(voxel_pools_.find(sp));
        if (itr != voxel_pools_.end())
        {
            return (*itr).second.get();
        }
    }

    {
        molecule_pool_map_type::iterator itr(molecule_pools_.find(sp));
        if (itr != molecule_pools_.end())
        {
            return (*itr).second.get();  // upcast
        }
    }

    const bool suc = make_molecular_type(sp, v.radius(), v.D(), v.loc());
    if (!suc)
    {
        throw IllegalState("never reach here");
    }

    molecule_pool_map_type::iterator i = molecule_pools_.find(sp);
    if (i == molecule_pools_.end())
    {
        throw IllegalState("never reach here");
    }
    return (*i).second.get();  // upcast
}

VoxelPool* LatticeSpaceVectorImpl::find_voxel_pool(const Species& sp)
{
    voxel_pool_map_type::iterator itr(voxel_pools_.find(sp));
    if (itr != voxel_pools_.end())
    {
        return (*itr).second.get();
    }
    return find_molecule_pool(sp);  // upcast
}

const VoxelPool* LatticeSpaceVectorImpl::find_voxel_pool(const Species& sp) const
{
    voxel_pool_map_type::const_iterator itr(voxel_pools_.find(sp));
    if (itr != voxel_pools_.end())
    {
        return (*itr).second.get();
    }
    return find_molecule_pool(sp);  // upcast
}

MoleculePool* LatticeSpaceVectorImpl::find_molecule_pool(const Species& sp)
{
    molecule_pool_map_type::iterator itr(molecule_pools_.find(sp));
    if (itr != molecule_pools_.end())
    {
        return (*itr).second.get();  // upcast
    }
    throw NotFound("MoleculePool not found.");
}

const MoleculePool* LatticeSpaceVectorImpl::find_molecule_pool(const Species& sp) const
{
    molecule_pool_map_type::const_iterator itr(molecule_pools_.find(sp));
    if (itr != molecule_pools_.end())
    {
        return (*itr).second.get();  // upcast
    }
    throw NotFound("MoleculePool not found.");
}

bool LatticeSpaceVectorImpl::on_structure(const Voxel& v)
{
    // return find_voxel_pool(v.coordinate()) != get_voxel_pool(v)->location();
    return voxels_.at(v.coordinate()) != get_voxel_pool(v)->location();
}

/*
 * Protected functions
 */

LatticeSpaceVectorImpl::coordinate_type LatticeSpaceVectorImpl::get_coord(
    const ParticleID& pid) const
{
    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        const boost::shared_ptr<MoleculePool>& mt((*itr).second);
        for (MoleculePool::const_iterator vitr(mt->begin());
             vitr != mt->end(); ++vitr)
        {
            if ((*vitr).pid == pid)
            {
                return (*vitr).coordinate;
            }
        }
    }
    return -1; //XXX: a bit dirty way
}

VoxelPool* LatticeSpaceVectorImpl::find_voxel_pool(const coordinate_type& coord) const
{
    return voxels_.at(coord);
}

// bool LatticeSpaceVectorImpl::has_species_exact(const Species& sp) const
// {
//     return spmap_.find(sp) != spmap_.end();
// }

bool LatticeSpaceVectorImpl::has_species(const Species& sp) const
{
    return (voxel_pools_.find(sp) != voxel_pools_.end()
            || molecule_pools_.find(sp) != molecule_pools_.end());
}

bool LatticeSpaceVectorImpl::remove_voxel(const ParticleID& pid)
{
    for (molecule_pool_map_type::iterator i(molecule_pools_.begin());
         i != molecule_pools_.end(); ++i)
    {
        const boost::shared_ptr<MoleculePool>& mt((*i).second);
        MoleculePool::const_iterator j(mt->find(pid));
        if (j != mt->end())
        {
            const coordinate_type coord((*j).coordinate);
            if (!mt->remove_voxel_if_exists(coord))
            {
                return false;
            }

            voxel_container::iterator itr(voxels_.begin() + coord);
            (*itr) = mt->location();
            mt->location()->add_voxel_without_checking(
                coordinate_id_pair_type(ParticleID(), coord));
            return true;
        }
    }
    return false;
}

bool LatticeSpaceVectorImpl::remove_voxel(const coordinate_type& coord)
{
    voxel_container::iterator itr(voxels_.begin() + coord);
    VoxelPool* mt(*itr);
    if (mt->is_vacant())
    {
        return false;
    }
    if (mt->remove_voxel_if_exists(coord))
    {
        (*itr) = mt->location();
        mt->location()->add_voxel_without_checking(
            coordinate_id_pair_type(ParticleID(), coord));
        return true;
    }
    return false;
}

bool LatticeSpaceVectorImpl::move(
    const coordinate_type& src, const coordinate_type& dest, const std::size_t candidate)
{
    return move_(src, dest, candidate).second;
}

bool LatticeSpaceVectorImpl::can_move(
    const coordinate_type& src, const coordinate_type& dest) const
{
    if (src == dest)
        return false;

    const VoxelPool* src_mt(voxels_.at(src));
    if (src_mt->is_vacant())
        return false;

    VoxelPool* dest_mt(voxels_.at(dest));

    if (dest_mt == border_)
        return false;

    if (dest_mt == periodic_)
        dest_mt = voxels_.at(apply_boundary_(dest));

    return (dest_mt == src_mt->location());
}

std::pair<LatticeSpaceVectorImpl::coordinate_type, bool>
    LatticeSpaceVectorImpl::move_to_neighbor(
        coordinate_type coord, Integer nrand)
{
    const coordinate_type neighbor(get_neighbor(coord, nrand));
    return move_(coord, neighbor);
}

std::pair<LatticeSpaceVectorImpl::coordinate_type, bool>
    LatticeSpaceVectorImpl::move_to_neighbor(
        coordinate_id_pair_type& info, Integer nrand)
{
    const coordinate_type neighbor(get_neighbor(info.coordinate, nrand));
    return move_(info, neighbor);
}

std::pair<LatticeSpaceVectorImpl::coordinate_type, bool>
    LatticeSpaceVectorImpl::move_(
        coordinate_type from, coordinate_type to,
        const std::size_t candidate)
{
    if (from == to)
    {
        // std::cerr << " from == to ";
        return std::pair<coordinate_type, bool>(from, false);
    }

    VoxelPool* from_mt(voxels_.at(from));
    if (from_mt->is_vacant())
    {
        return std::pair<coordinate_type, bool>(from, true);
    }

    VoxelPool* to_mt(voxels_.at(to));

    if (to_mt == border_)
    {
        // std::cerr << " to_mt is border ";
        return std::pair<coordinate_type, bool>(from, false);
    }
    else if (to_mt == periodic_)
    {
        to = apply_boundary_(to);
        to_mt = voxels_.at(to);
    }

    if (to_mt != from_mt->location())
    {
        // std::cerr << " to_mt is " << to_mt->species().serial() << " ";
        return std::pair<coordinate_type, bool>(to, false);
    }

    from_mt->replace_voxel(from, to, candidate);
    voxel_container::iterator from_itr(voxels_.begin() + from);
    (*from_itr) = to_mt;

    // to_mt->replace_voxel(to, coordinate_id_pair_type(ParticleID(), from));
    to_mt->replace_voxel(to, from);
    voxel_container::iterator to_itr(voxels_.begin() + to);
    (*to_itr) = from_mt;

    return std::pair<coordinate_type, bool>(to, true);
}

std::pair<LatticeSpaceVectorImpl::coordinate_type, bool>
    LatticeSpaceVectorImpl::move_(
        coordinate_id_pair_type& info, coordinate_type to)
{
    const coordinate_type from(info.coordinate);
    if (from == to)
    {
        return std::pair<coordinate_type, bool>(from, false);
    }

    VoxelPool* from_mt(voxels_.at(from));
    if (from_mt->is_vacant())
    {
        return std::pair<coordinate_type, bool>(from, true);
    }

    VoxelPool* to_mt(voxels_.at(to));

    if (to_mt == border_)
    {
        return std::pair<coordinate_type, bool>(from, false);
    }
    else if (to_mt == periodic_)
    {
        to = apply_boundary_(to);
        to_mt = voxels_.at(to);
    }

    if (to_mt != from_mt->location())
    {
        return std::pair<coordinate_type, bool>(to, false);
    }

    info.coordinate = to;
    voxel_container::iterator from_itr(voxels_.begin() + from);
    (*from_itr) = to_mt;

    // to_mt->replace_voxel(to, coordinate_id_pair_type(ParticleID(), from));
    to_mt->replace_voxel(to, from);
    voxel_container::iterator to_itr(voxels_.begin() + to);
    (*to_itr) = from_mt;

    return std::pair<coordinate_type, bool>(to, true);
}

std::pair<LatticeSpaceVectorImpl::coordinate_type, bool>
    LatticeSpaceVectorImpl::move_to_neighbor(
        VoxelPool* const& from_mt, VoxelPool* const& loc,
        coordinate_id_pair_type& info, const Integer nrand)
{
    const coordinate_type from(info.coordinate);
    coordinate_type to(get_neighbor(from, nrand));

    //XXX: assert(from != to);
    //XXX: assert(from_mt == voxels_[from]);
    //XXX: assert(from_mt != vacant_);

    VoxelPool* to_mt(voxels_[to]);

    if (to_mt != loc)
    {
        if (to_mt == border_)
        {
            return std::make_pair(from, false);
        }
        else if (to_mt != periodic_)
        {
            return std::make_pair(to, false);
        }

        // to_mt == periodic_
        to = apply_boundary_(to);
        to_mt = voxels_[to];

        if (to_mt != loc)
        {
            return std::make_pair(to, false);
        }
    }

    voxels_[from] = to_mt;
    voxels_[to] = from_mt;
    info.coordinate = to; //XXX: updating data

    to_mt->replace_voxel(to, from);
    // if (to_mt != vacant_) // (!to_mt->is_vacant())
    // {
    //     to_mt->replace_voxel(
    //         to, coordinate_id_pair_type(ParticleID(), from));
    // }
    return std::make_pair(to, true);
}

const Particle LatticeSpaceVectorImpl::particle_at(
    const coordinate_type& coord) const
{
    const VoxelPool* mt(voxels_.at(coord));
    return Particle(
        mt->species(),
        coordinate2position(coord),
        mt->radius(), mt->D());
}

Integer LatticeSpaceVectorImpl::num_voxels_exact(const Species& sp) const
{
    {
        voxel_pool_map_type::const_iterator itr(voxel_pools_.find(sp));
        if (itr != voxel_pools_.end())
        {
            const boost::shared_ptr<VoxelPool>& mt((*itr).second);
            return count_voxels(mt);
        }
    }

    {
        molecule_pool_map_type::const_iterator itr(molecule_pools_.find(sp));
        if (itr != molecule_pools_.end())
        {
            const boost::shared_ptr<MoleculePool>& mt((*itr).second);
            return mt->size();  // upcast
        }
    }

    return 0;
}

Integer LatticeSpaceVectorImpl::num_voxels(const Species& sp) const
{
    Integer count(0);
    SpeciesExpressionMatcher sexp(sp);

    for (voxel_pool_map_type::const_iterator itr(voxel_pools_.begin());
         itr != voxel_pools_.end(); ++itr)
    {
        if (sexp.match((*itr).first))
        {
            const boost::shared_ptr<VoxelPool>& mt((*itr).second);
            count += count_voxels(mt);
        }
    }

    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        if (sexp.match((*itr).first))
        {
            const boost::shared_ptr<MoleculePool>& mt((*itr).second);
            count += mt->size();
        }
    }
    return count;
}

Integer LatticeSpaceVectorImpl::num_voxels() const
{
    Integer count(0);

    for (voxel_pool_map_type::const_iterator itr(voxel_pools_.begin());
         itr != voxel_pools_.end(); ++itr)
    {
        const boost::shared_ptr<VoxelPool>& mt((*itr).second);
        count += count_voxels(mt);
    }

    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        const boost::shared_ptr<MoleculePool>& mt((*itr).second);
        count += mt->size();
    }
    return count;
}

// /**
//  * Change the Species at v.coordinate() to v.species.
//  * The ParticleID must be kept after this update.
//  */
// void LatticeSpaceVectorImpl::update_voxel(const Voxel& v)
// {
//     const coordinate_type coord(v.coordinate());
//     VoxelPool* src_mt(voxels_.at(coord));
//     VoxelPool* new_mt(get_voxel_pool(v)); //XXX: need MoleculeInfo
// 
//     if (src_mt->with_voxels() != new_mt->with_voxels())
//     {
//         throw NotSupported("ParticleID is needed/lost.");
//     }
// 
//     new_mt->add_voxel_without_checking(src_mt->pop(coord));
//     voxel_container::iterator itr(voxels_.begin() + coord);
//     (*itr) = new_mt;
// }

/*
 * Change the Species and coordinate of a Voxel with ParticleID, pid, to
 * v.species() and v.coordinate() respectively and return false.
 * If no Voxel with pid is found, create a new Voxel at v.coordiante() and return ture.
 */
bool LatticeSpaceVectorImpl::update_voxel(const ParticleID& pid, const Voxel& v)
{
    const LatticeSpaceVectorImpl::coordinate_type& to_coord(v.coordinate());
    if (!is_in_range(to_coord))
    {
        throw NotSupported("Out of bounds");
    }

    VoxelPool* new_mt(get_voxel_pool(v)); //XXX: need MoleculeInfo
    VoxelPool* dest_mt(find_voxel_pool(to_coord));

    if (dest_mt != new_mt->location())
    {
        throw NotSupported(
            "Mismatch in the location. Failed to place '"
            + new_mt->species().serial() + "' to '"
            + dest_mt->species().serial() + "'.");
    }

    const LatticeSpaceVectorImpl::coordinate_type
        from_coord(pid != ParticleID() ? get_coord(pid) : -1);
    if (from_coord != -1)
    {
        // move
        VoxelPool* src_mt(voxels_.at(from_coord));
        src_mt->remove_voxel_if_exists(from_coord);

        //XXX: use location?
        dest_mt->replace_voxel(to_coord, from_coord);
        voxel_container::iterator from_itr(voxels_.begin() + from_coord);
        (*from_itr) = dest_mt;

        new_mt->add_voxel_without_checking(coordinate_id_pair_type(pid, to_coord));
        voxel_container::iterator to_itr(voxels_.begin() + to_coord);
        (*to_itr) = new_mt;
        return false;
    }

    // new
    dest_mt->remove_voxel_if_exists(to_coord);

    new_mt->add_voxel_without_checking(coordinate_id_pair_type(pid, to_coord));
    voxel_container::iterator to_itr(voxels_.begin() + to_coord);
    (*to_itr) = new_mt;
    return true;
}

bool LatticeSpaceVectorImpl::make_structure_type(const Species& sp,
    Shape::dimension_kind dimension, const std::string loc)
{
    voxel_pool_map_type::iterator itr(voxel_pools_.find(sp));
    if (itr != voxel_pools_.end())
    {
        return false;
    }
    else if (molecule_pools_.find(sp) != molecule_pools_.end())
    {
        throw IllegalState(
            "The given species is already assigned to the MoleculePool.");
    }

    VoxelPool* location;
    if (loc == "")
    {
        location = vacant_;
    }
    else
    {
        const Species locsp(loc);
        try
        {
            location = find_voxel_pool(locsp);
        }
        catch (const NotFound& err)
        {
            // XXX: A VoxelPool for the structure (location) must be allocated
            // XXX: before the allocation of a Species on the structure.
            // XXX: The VoxelPool cannot be automatically allocated at the time
            // XXX: because its MoleculeInfo is unknown.
            // XXX: LatticeSpaceVectorImpl::load will raise a problem about this issue.
            // XXX: In this implementation, the VoxelPool for a structure is
            // XXX: created with default arguments.
            boost::shared_ptr<MoleculePool>
                locmt(new MolecularType(locsp, vacant_, voxel_radius_, 0));
            std::pair<molecule_pool_map_type::iterator, bool>
                locval(molecule_pools_.insert(
                    molecule_pool_map_type::value_type(locsp, locmt)));
            if (!locval.second)
            {
                throw AlreadyExists(
                    "never reach here. make_structure_type seems wrong.");
            }
            location = (*locval.first).second.get();
        }
    }

    boost::shared_ptr<VoxelPool>
        mt(new StructureType(sp, location, voxel_radius_, dimension));
    std::pair<voxel_pool_map_type::iterator, bool>
        retval(voxel_pools_.insert(voxel_pool_map_type::value_type(sp, mt)));
    if (!retval.second)
    {
        throw AlreadyExists("never reach here.");
    }
    return retval.second;
}

bool LatticeSpaceVectorImpl::make_interface_type(const Species& sp,
    Shape::dimension_kind dimension, const std::string loc)
{
    voxel_pool_map_type::iterator itr(voxel_pools_.find(sp));
    if (itr != voxel_pools_.end())
    {
        return false;
    }
    else if (molecule_pools_.find(sp) != molecule_pools_.end())
    {
        throw IllegalState(
            "The given species is already assigned to the MoleculePool.");
    }

    VoxelPool* location;
    if (loc == "")
    {
        location = vacant_;
    }
    else
    {
        const Species locsp(loc);
        try
        {
            location = find_voxel_pool(locsp);
        }
        catch (const NotFound& err)
        {
            // XXX: A VoxelPool for the structure (location) must be allocated
            // XXX: before the allocation of a Species on the structure.
            // XXX: The VoxelPool cannot be automatically allocated at the time
            // XXX: because its MoleculeInfo is unknown.
            // XXX: LatticeSpaceVectorImpl::load will raise a problem about this issue.
            // XXX: In this implementation, the VoxelPool for a structure is
            // XXX: created with default arguments.
            boost::shared_ptr<MoleculePool>
                locmt(new MolecularType(locsp, vacant_, voxel_radius_, 0));
            std::pair<molecule_pool_map_type::iterator, bool>
                locval(molecule_pools_.insert(
                    molecule_pool_map_type::value_type(locsp, locmt)));
            if (!locval.second)
            {
                throw AlreadyExists(
                    "never reach here. make_interface_type seems wrong.");
            }
            location = (*locval.first).second.get();
        }
    }

    boost::shared_ptr<VoxelPool>
        mt(new InterfaceType(sp, location, voxel_radius_, dimension));
    std::pair<voxel_pool_map_type::iterator, bool>
        retval(voxel_pools_.insert(voxel_pool_map_type::value_type(sp, mt)));
    if (!retval.second)
    {
        throw AlreadyExists("never reach here.");
    }
    return retval.second;
}

bool LatticeSpaceVectorImpl::make_molecular_type(const Species& sp, Real radius, Real D, const std::string loc)
{
    molecule_pool_map_type::iterator itr(molecule_pools_.find(sp));
    if (itr != molecule_pools_.end())
    {
        return false;
    }
    else if (voxel_pools_.find(sp) != voxel_pools_.end())
    {
        throw IllegalState(
            "The given species is already assigned to the VoxelPool with no voxels.");
    }

    VoxelPool* location;
    if (loc == "")
    {
        location = vacant_;
    }
    else
    {
        const Species locsp(loc);
        try
        {
            location = find_voxel_pool(locsp);
        }
        catch (const NotFound& err)
        {
            // XXX: A VoxelPool for the structure (location) must be allocated
            // XXX: before the allocation of a Species on the structure.
            // XXX: The VoxelPool cannot be automatically allocated at the time
            // XXX: because its MoleculeInfo is unknown.
            // XXX: LatticeSpaceVectorImpl::load will raise a problem about this issue.
            // XXX: In this implementation, the VoxelPool for a structure is
            // XXX: created with default arguments.
            boost::shared_ptr<MoleculePool>
                locmt(new MolecularType(locsp, vacant_, voxel_radius_, 0));
            std::pair<molecule_pool_map_type::iterator, bool>
                locval(molecule_pools_.insert(
                    molecule_pool_map_type::value_type(locsp, locmt)));
            if (!locval.second)
            {
                throw AlreadyExists(
                    "never reach here. find_voxel_pool seems wrong.");
            }
            location = (*locval.first).second.get();
        }
    }

    boost::shared_ptr<MoleculePool>
        mt(new MolecularType(sp, location, radius, D));
    std::pair<molecule_pool_map_type::iterator, bool>
        retval(molecule_pools_.insert(
            molecule_pool_map_type::value_type(sp, mt)));
    if (!retval.second)
    {
        throw AlreadyExists("never reach here.");
    }
    return retval.second;
}

bool LatticeSpaceVectorImpl::add_voxels(const Species sp, std::vector<std::pair<ParticleID, coordinate_type> > voxels)
{
    // this function doesn't check location.
    VoxelPool *mtb;
    try
    {
        mtb = find_voxel_pool(sp);
    }
    catch (NotFound &e)
    {
        return false;
    }
    for (std::vector<std::pair<ParticleID, coordinate_type> >::iterator itr(voxels.begin());
            itr != voxels.end(); ++itr)
    {
        const ParticleID pid((*itr).first);
        const coordinate_type coord((*itr).second);
        VoxelPool* src_mt(find_voxel_pool(coord));
        src_mt->remove_voxel_if_exists(coord);
        mtb->add_voxel_without_checking(coordinate_id_pair_type(pid, coord));
        voxel_container::iterator vitr(voxels_.begin() + coord);
        (*vitr) = mtb;
    }
    return true;
}

Integer LatticeSpaceVectorImpl::count_voxels(const boost::shared_ptr<VoxelPool>& mt) const
{
    return static_cast<Integer>(
        std::count(voxels_.begin(), voxels_.end(), mt.get()));
}

} // ecell4
