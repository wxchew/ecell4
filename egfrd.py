#!/usr/env python


import math
import bisect
import random
import sys

import numpy
import scipy
import scipy.optimize


from utils import *
from surface import *
import gfrdfunctions
import _gfrd

from gfrdbase import *



class EGFRDSimulator( GFRDSimulatorBase ):
    
    def __init__( self ):
        GFRDSimulatorBase.__init__( self )



    def initialize( self ):
        self.clear()

        self.formPairs()

        self.assignFirstPassageTime()

    def step( self ):
    
        self.clear()

        # proceed slightly even if dtMax is 0.
        #if self.dtMax <= 1e-18:
        #    self.dtMax = 1e-18
        
        self.determineNextReaction()

        print 'maxdt', self.dtMax, 'dt', self.dt,\
              'reactions', self.reactionEvents,\
              'rejected moves', self.rejectedMoves
        
        self.propagateParticles()

        self.t += self.dt


    def clear( self ):

        self.dtMax = INF
        self.dt = INF

        self.nextReaction = None

        self.pairs = []
        self.singles = []


    def assignFirstPassageTime( self ):
        
        for single in self.singles:
            single.dt = self.calculateFirstPassageTime1( single )

    def calculateFirstPassageTime1( self, single ):
        
        species = self.speciesList.values()[single.si]
        fpgf = _gfrd.FirstPassageGreensFunction( species.D )
        print single.dr
        return fpgf.drawTime( random.random(), single.dr[0] )

    def isPopulationChanged( self ):
        return self.nextReaction != None


    def determineNextReaction( self ):

        self.dt = self.dtMax

        # first order reactions
        for rt in self.reactionTypeList1.values():
            reactantSpecies = rt.reactants[0]

            pool = reactantSpecies.pool

            dt, i = self.nextReactionTime1( rt, pool )
            
            reaction1 = ( dt, reactantSpecies, i, rt )

            if i != None:
            
                dt1 = reaction1[0]

                if self.dt >= dt1:
                    self.dt = dt1
                    self.nextReaction = reaction1

                

        # second order reactions

        for pair in self.pairs:
            pair.rdt = self.nextReactionTime2( pair )

        #self.pairs = [ ( self.nextReactionTime2( r ),\
        #r[0], r[1], r[2], r[3], r[4], r[5], r[6] )\
        #                       for r in self.pairs ]

        if len( self.pairs ) != 0:
            
            self.pairs.sort(key=operator.attrgetter('rdt'))
            pair = self.pairs[0]
            dt2 = pair.rdt  # reaction2[0]

            if self.dt >= dt2:

                self.dt = dt2
                self.nextReaction = pair

        print 'next reaction = ', self.nextReaction


    def nextReactionTime1( self, rt, pool ):

        if pool.size == 0:
            return None, None

        dts = numpy.array( [ random.random() for i in range( pool.size ) ] )
        dts = - numpy.log( dts ) / rt.k

        i = numpy.argmin( dts )

        return dts[i], i 
        


    def nextReactionTime2( self, pair ):

        if pair.rt == None:
            return INF

        species1 = self.speciesList.values()[pair.si1]
        species2 = self.speciesList.values()[pair.si2]
        pos1 = species1.pool.positions[pair.i1]
        pos2 = species2.pool.positions[pair.i2]

        radius = species1.radius + species2.radius
        r0 = self.distance( pos1, pos2 )

        if radius > r0:
            print 'CRITICAL: radius > r0', str(radius), str(r0)
            #            return scipy.Inf
            print pair
            print pair.rt.str()
            #sys.exit(-1)

        u = random.random()


        res = pair.rt.pairGreensFunction.drawTime( u, r0, self.dtMax )

        return res



    def fireReaction1( self, reaction ):

        dt, reactantSpecies, index, rt = reaction
        
        pos = reactantSpecies.pool.positions[index].copy()

        if len( rt.products ) == 1:
            productSpecies = rt.products[0]

            reactantSpecies.removeParticleByIndex( index )

            if reactantSpecies.radius < productSpecies.radius and \
                   not self.checkOverlap( pos, productSpecies1.radius ):
                raise 'fireReaction1: overlap check failed'

            productSpecies.newParticle( pos )

        elif len( rt.products ) == 2:
            
            productSpecies1 = rt.products[0]
            productSpecies2 = rt.products[1]

            D1 = productSpecies1.D
            D2 = productSpecies2.D

            reactantSpecies.removeParticleByIndex( index )

            unitVector = randomUnitVector()

            #print 'unit', self.distance( unitVector, numpy.array([0,0,0]) )
            distance = productSpecies1.radius + productSpecies2.radius
            vector = unitVector * ( distance * ( 1.0 + 1e-2 ) ) # safety

            newpos1 = pos + vector * ( D1 / (D1 + D2) )
            newpos2 = pos - vector * ( D2 / (D1 + D2) )

            #FIXME: SURFACE
            newpos1 %= self.fsize
            newpos2 %= self.fsize
            
            # debug
            d = self.distance( newpos1, newpos2 )
            if d < distance:
                raise "d = %s, %s" %( d, distance)

            productSpecies1.newParticle( newpos1 )
            productSpecies2.newParticle( newpos2 )

        elif len( rt.products ) == 0:

            reactantSpecies.removeParticleByIndex( index )

        else:
            raise "num products >= 3 not supported."

            
    def fireReaction2( self, pair ):

        print 'fire:', pair

        #dt, pdt, ndt, speciesIndex1, index1, speciesIndex2, index2, rt = pair

        species1 = self.speciesList.values()[pair.si1]
        species2 = self.speciesList.values()[pair.si2]

        pos1 = species1.pool.positions[ pair.i1 ].copy()
        pos2 = species2.pool.positions[ pair.i2 ].copy()

        D1 = species1.D
        D2 = species2.D

        if len( pair.rt.products ) == 1:

            species3 = pair.rt.products[0]

            serial1 = species1.pool.getSerialByIndex( pair.i1 )
            serial2 = species2.pool.getSerialByIndex( pair.i2 )

            if D1 == 0.0:
                newpos = pos1
            elif D2 == 0.0:
                newpos = pos2
            else:
                
                sqrtD2D1 = math.sqrt( D2 / D1 ) 
                sqrtD1D2 = math.sqrt( D1 / D2 )
                
                R0 = sqrtD2D1 * pos1 + sqrtD1D2 * pos2
                
                dR = gfrdfunctions.p2_R( D1, D2, self.dt )
                newpos = ( R0 + dR ) / ( sqrtD1D2 + sqrtD2D1 )
                
                
            #FIXME: SURFACE
            newpos %= self.fsize

                
            species1.removeParticleBySerial( serial1 )
            species2.removeParticleBySerial( serial2 )

            # debug
            self.checkOverlap( newpos, species3.radius )

            species3.newParticle( newpos )


    def propagateParticles( self ):

        self.propagateSingles()


        # fireReaction should come last in any case because
        # it can change particle identities, thus invalidate particle indices.

        if self.nextReaction == None:  # no reaction
            self.propagatePairs( self.pairs )
        elif len( self.pairs ) != 0 and\
                 self.nextReaction == self.pairs[0]: # binary reaction
            if len( self.pairs ) > 1: 
                self.propagatePairs( self.pairs[1:] )
            self.fireReaction2( self.nextReaction )
            self.reactionEvents += 1
        else:                           # unary reaction
            self.propagatePairs( self.pairs )
            self.fireReaction1( self.nextReaction ) # after propagatePairs()
            self.reactionEvents += 1

    def propagateSingles( self ):


        for i in range( len( self.singles ) ):
            species = self.speciesList.values()[i]

            if species.D != 0.0:
                for j in self.singles[i]:

                    self.simpleDiffusion( i, j )


    def propagatePairs( self, pairs ):

        for pair in pairs:

            print pair
            #dt, pdt, ndt, speciesIndex1, i1, speciesIndex2, i2, rt = pair

            species1 = self.speciesList.values()[pair.si1]
            species2 = self.speciesList.values()[pair.si2]

            D1 = species1.D
            D2 = species2.D

            radius1 = species1.radius
            radius2 = species2.radius

            #debug
            if D1 == 0.0 and D1 == D2:
                raise "unexpected: D1 == D2 == 0.0"

            pos1 = species1.pool.positions[pair.i1]
            pos2 = species2.pool.positions[pair.i2]

            if True: # cyclic boundary; temporary displace particles.
                disposition = ( pos2 - pos1 ) > ( self.fsize * 0.5 )
                pos2 -= disposition * self.fsize
                disposition = ( pos1 - pos2 ) > ( self.fsize * 0.5 )
                pos1 -= disposition * self.fsize


            r0 = self.distance( pos1, pos2 )

            interParticle = pos2 - pos1
            #            interParticleS = cartesianToSpherical( interParticle )

            limit1 = self.H * math.sqrt( 6.0 * D1 * self.dt )
            limit2 = self.H * math.sqrt( 6.0 * D2 * self.dt )
                                     
            # if particles are far apart use simpleDiffusion()
            correlationLimit = limit1 + limit2 + radius1 + radius2

            if r0 > correlationLimit:
                print '== simple diffusion =='
                self.simpleDiffusion( pair.si1, pair.i1 )
                self.simpleDiffusion( pair.si2, pair.i2 )
                continue

            if D1 == 0.0:
                sqrtD1D2 = 0.0
                sqrtD2D1 = 1.0
                R = pos1
            elif D2 == 0.0:
                # need to check if this procedure is stable and
                # pos2 remains immobile.
                sqrtD1D2 = 1.0
                sqrtD2D1 = 0.0
                R = pos1
            else:
                sqrtD1D2 = math.sqrt( D1 / D2 )
                sqrtD2D1 = math.sqrt( D2 / D1 ) 
                R0 = sqrtD2D1 * pos1 + sqrtD1D2 * pos2
                dR = gfrdfunctions.p2_R( D1, D2, self.dt )
                R = R0 + dR

            while True:

                r =pair.rt.pairGreensFunction.drawR( random.random(), r0, self.dt )
                
                theta = pair.rt.pairGreensFunction.drawTheta( random.random(),\
                                                              r, r0, self.dt )
                phi = random.random() * 2.0 * Pi
                
                # new inter particle vector
                newInterParticleS = numpy.array( [ r, theta, phi ] )
                newInterParticle = sphericalToCartesian( newInterParticleS )
                
                # Now I rotate this new interparticle vector along the
                # rotation axis that is perpendicular to both the
                # z-axis and the original interparticle vector for
                # the angle between these.
                
                # the rotation axis is a normalized cross product of
                # the z-axis and the original vector.
                # rotationAxis2 = crossproduct( [ 0,0,1 ], interParticle )
                rotationAxis = crossproductAgainstZAxis( interParticle )
                rotationAxis = normalize( rotationAxis )
                
                angle = vectorAngleAgainstZAxis( interParticle )
                
                newInterParticle = rotateVector( newInterParticle,
                                                 rotationAxis,
                                                 angle )

                newpos1 = ( R - sqrtD1D2 * newInterParticle ) \
                          / ( sqrtD1D2 + sqrtD2D1 )
                
                newpos2 = newpos1 + newInterParticle
                
                newDistance11 = distance( newpos1, pos1 )
                newDistance22 = distance( newpos2, pos2 )
                newParticleDistance = distance( newpos1, newpos2 )
                if limit1 >= newDistance11 and \
                       limit2 >= newDistance22 and \
                       newParticleDistance > radius1 + radius2:
                    break
                
                print 'rejected move: ',\
                      'lim1, dist11', limit1, newDistance11,\
                      'lim2, dist22', limit2, newDistance22,\
                      'radii, interp', radius1 + radius2, newParticleDistance
                print 'DEBUG: r0, dt, pos1, pos2, newpos1, newpos2',\
                      r0, self.dt, pos1, pos2, newpos1, newpos2
                
                self.rejectedMoves += 1
                raise 'stop'


            #FIXME: SURFACE
            newpos1 %= self.fsize
            newpos2 %= self.fsize

            species1.pool.positions[pair.i1] = newpos1
            species2.pool.positions[pair.i2] = newpos2




    def formPairs( self ):

        # 1. form pairs in self.pairs
        # 2. list singles in self.singles

        speciesList = self.speciesList.values()

        # list up pair candidates

        # partner -> nearest particle
        # neighbor -> second nearest particle

        # dtCache[ speciesIndex ][ particleIndex ][ 0 .. 1 ]
        dtCache = []
        drCache = []
        neighborCache = []
        checklist = []

        for speciesIndex in range( len( speciesList ) ):
            size = speciesList[speciesIndex].pool.size

            dtCache.append( numpy.zeros( ( size, 2 ), numpy.floating ) )
            drCache.append( numpy.zeros( ( size, 2 ), numpy.floating ) )
            neighborCache.append( [[[ -1, -1 ],[-1,-1]]] * size )

            checklist.append( numpy.ones( speciesList[speciesIndex].pool.size ) )
            for particleIndex in range( size ):

                neighbors, dts, drs = self.checkPairs( speciesIndex,\
                                                       particleIndex )

                neighborCache[ speciesIndex ][ particleIndex ] = neighbors
                dtCache[ speciesIndex ][ particleIndex ] = dts
                drCache[ speciesIndex ][ particleIndex ] = drs

        self.pairs = []
        for speciesIndex1 in range( len( speciesList ) ):

            species1 = speciesList[speciesIndex1]

            for particleIndex1 in range( species1.pool.size ):

                # skip if this particle has already taken in a pair.
                if checklist[speciesIndex1][particleIndex1] == 0:
                    #print 'skip', speciesIndex1, particleIndex1
                    continue

                # A partner: the other of the pair.
                # A neighbor of a pair: closer of the second closest of
                #                       the particles in the pair.
                #                       This is different from neighbors of
                #                       a particle.
                
                # (1) Find the closest particle (partner).
                partner = neighborCache[ speciesIndex1 ][ particleIndex1 ][0]

                ( speciesIndex2, particleIndex2 ) = partner

                if speciesIndex2 == -1:
                    continue

                dts = dtCache[ speciesIndex1 ][ particleIndex1 ]

                partnersPartner = neighborCache\
                                  [ speciesIndex2 ][ particleIndex2 ][0]
                partnerDts = dtCache[ speciesIndex2 ][ particleIndex2 ]

                # (2) The partner's partner has to be this, otherwise
                #     this combination isn't a pair.
                # (3) 'Neighbor' of this pair is the closer of
                #     this and the partner's second closest.
                #     We take the particle that has this neighbor.
                if partnersPartner != ( speciesIndex1, particleIndex1 ) or \
                       partnerDts[1] < dts[1]:
                    continue
                
                # (4) Now we have a candidate pair.
                species2 = speciesList[speciesIndex2]
                rt = self.reactionTypeList2.get( ( species1, species2 ) )

                #pair = ( dts[0], dts[1], speciesIndex1, particleIndex1,\
                #speciesIndex2, particleIndex2, rt )
                pair = Pair( dts[0], dts[1], speciesIndex1, particleIndex1,\
                             speciesIndex2, particleIndex2, rt )
                self.pairs.append( pair )

                # (5) dtMax = the minimum neighbor dt of all pairs.
                self.dtMax = min( self.dtMax, dts[1] )

                # (6) book keeping
                checklist[speciesIndex1][particleIndex1] = 0
                checklist[speciesIndex2][particleIndex2] = 0



        # screening pairs
        self.pairs.sort(key=operator.attrgetter('dt'))

        checklist = []
        for i in range( len( speciesList ) ):
            checklist.append( numpy.ones( speciesList[i].pool.size ) )

        for i in range( len( self.pairs ) ):
            #( dt, ndt, si1, i1, si2, i2, rt ) = self.pairs[i]
            pair = self.pairs[i]


            # Don't take pairs with partner dt greater than dtMax.
            if pair.dt > self.dtMax:
                self.pairs = self.pairs[:i]
                break   # pairs are sorted by dt.  break here.

            if checklist[pair.si1][pair.i1] == 0 or \
                   checklist[pair.si2][pair.i2] == 0:
                print self.pairs[:i+1]
                print dtCache[pair.si1][pair.i1], dtCache[pair.si2][pair.i2]
                print neighborCache[pair.si1][pair.i1], \
                      neighborCache[pair.si2][pair.i2]
                print 'pairs not mutually exclusive.'
                self.pairs = self.pairs[:i]
                break

            checklist[pair.si1][pair.i1] = 0
            checklist[pair.si2][pair.i2] = 0


        # now we have the final list of pairs

        # next, make the list of singles.
        # a single is a particle that doesn't appear in the list
        # of pairs.
        self.singles = []

        for i in range( len( checklist ) ):
            singleIndices = numpy.nonzero( checklist[i] )[0]
            for j in singleIndices:
                self.singles.append( Single( INF, i, j, drCache[i][j] ) )
        #    singleIndices = numpy.nonzero( checklist[i] )[0] #== flatnonzero()
        #    self.singles.append( singleIndices )

        #debug
        numSingles = len( self.singles )

        print '# pairs = ', len(self.pairs),\
              ', # singles = ', numSingles




    def checkPairs( self, speciesIndex1, particleIndex ):

        speciesList = self.speciesList.values()

        #           [ ( species, particle ), (...,...), .. ]
        neighbors = [ ( -1, -1 ), ( -1, -1 ) ]
        neighborDts = [ INF, INF ]
        neighborDrs = [ INF, INF ]

        species1 = speciesList[ speciesIndex1 ]
        positions = species1.pool.positions
        position1 = positions[ particleIndex ].copy()

        if self.reactionTypeList2.get( ( species1, species1 ), None ) != None \
           and len( position1 ) >= 2 and species1.D != 0.0:

            # temporarily displace the particle
            positions[particleIndex] = NOWHERE
            
            indices, dts, drs = self.checkDistance( position1, positions,
                                                    species1, species1 )

            # restore the particle.
            positions[particleIndex] = position1

            neighborDts.extend( dts )
            neighborDrs.extend( drs )

            if len( indices ) == 2:
                neighbors.extend( ( ( speciesIndex1, indices[0] ),\
                                    ( speciesIndex1, indices[1] ) ) )
            else:
                neighbors.extend( ( ( speciesIndex1, indices[0] ), ) )

        #for speciesIndex2 in range( speciesIndex1 + 1, len( speciesList ) ):
        for speciesIndex2 in range( speciesIndex1 )\
                + range( speciesIndex1 + 1, len( speciesList ) ):
            species2 = speciesList[speciesIndex2]

            # non reactive
            if self.reactionTypeList2.get( ( species1, species2 ), None )\
                   == None:
                continue
            
            if species2.pool.size == 0:
                continue

            if species1.D + species2.D == 0.0:
                continue
                    
            positions = species2.pool.positions

            if species2.pool.size == 1:  # insert a dummy
                positions = numpy.concatenate( ( positions, [NOWHERE,] ) )
                
            indices, dts, drs = self.checkDistance( position1, positions,
                                                    species1, species2 )
            neighborDts.extend( dts )
            neighborDrs.extend( drs )
            neighbors.extend( ( ( speciesIndex2, indices[0] ),\
                                ( speciesIndex2, indices[1] ) ) )

        topargs = numpy.argsort( neighborDts )[:2]
        topNeighborDts = numpy.take( neighborDts, topargs )
        topNeighborDrs = numpy.take( neighborDrs, topargs )
        topNeighbors = ( neighbors[topargs[0]], neighbors[topargs[1]] )

        return topNeighbors, topNeighborDts, topNeighborDrs


    def checkDistance( self, position1, positions2, species1, species2 ):

        #positions2 = species2.pool.positions

        distanceSq = self.distanceSqArray( position1, positions2 )
        sortedindices = distanceSq.argsort()

        #debug
        radius12 = species1.radius + species2.radius
        radius12sq = radius12 * radius12

        # check if particles overlap
        if distanceSq[ sortedindices[0] ] < radius12sq - 1e-20 and \
               distanceSq[ sortedindices[0] ] != 0.0:
            print position1, positions2[ sortedindices[0] ]
            print 'dr<radius', math.sqrt(distanceSq[sortedindices[0]]), radius12
            print species1.id, species2.id, sortedindices[0]
            raise "critical"

        factor = ( math.sqrt( species1.D ) + math.sqrt( species2.D ) ) * self.H
        factor *= factor
        factor *= 6.0

        distanceSqSorted = distanceSq.take( sortedindices[:2] )
        distances = numpy.sqrt( distanceSqSorted )
        # instead of just
        #dts = distanceSqSorted / factor
        # below takes into account of particle radii.
        dts = distances - radius12
        dts *= dts
        dts /= factor

        #if min( dts ) < 0.0:
        #print 'negative dts occured, clipping.', dts
        #dts = numpy.clip( dts, 0.0, INF )
        # raise 'stop'

        drs = self.H * numpy.sqrt( 6.0 * species1.D * dts )

        indices = sortedindices[:2]

        return indices, dts, drs

