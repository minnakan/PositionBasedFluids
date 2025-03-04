#include "PBFSystem.h"
#include <iostream>
#include <random>
#include <glm/gtx/norm.hpp>

PBFSystem::PBFSystem()
{
    // Keep your original parameters but adjust key ones for better settling
    dt = 0.008f;                  // Keep your current timestep
    h = 0.4f;                     // Keep your current kernel radius
    solverIterations = 5;         // Keep current iterations

    // Critical changes for settling:
    relaxationFactor = 0.0001f;   // Lower this for better constraint solving
    artificalPressureK = 0.001f;  // Lower this for less repulsion
    boundaryDamping = 0.5f;       // Increase for more damping at boundaries

    // Keep your other parameters
    vorticityStrength = 0.05f;
    xsphC = 0.005f;

}

void PBFSystem::initScene()
{
    particles.clear();

    // Create a block of particles for fluid simulation
    const int numX = 10;
    const int numY = 10;
    const int numZ = 10;
    float spacing = particleRadius * 1.8f; // Slightly closer together for better cohesion

    // Start position - higher up for more stable initialization
    glm::vec3 start(-numX * spacing * 0.5f, 3.0f, -numZ * spacing * 0.5f);

    // Minimal jitter to maintain structure initially
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.005f, 0.005f);

    // Add particles in a block formation
    for (int x = 0; x < numX; ++x)
    {
        for (int y = 0; y < numY; ++y)
        {
            for (int z = 0; z < numZ; ++z)
            {
                Particle p;
                p.position = start + glm::vec3(
                    x * spacing + jitter(gen),
                    y * spacing + jitter(gen),
                    z * spacing + jitter(gen)
                );
                p.predictedPosition = p.position;  // Ensure both positions start the same
                p.velocity = glm::vec3(0.0f);
                p.density = 0.0f;
                p.lambda = 0.0f;
                p.deltaP = glm::vec3(0.0f);

                // Color based on height for better visualization
                float b = 0.5f + 0.5f * (float)y / numY;
                p.color = glm::vec3(0.1f, 0.3f, b);

                particles.push_back(p);
            }
        }
    }

    std::cout << "Created " << particles.size() << " particles" << std::endl;
}

void PBFSystem::step()
{
    // Apply external forces and predict positions
    predictPositions();

    // Handle boundaries for predicted positions immediately
    enforceBoxBoundaries(true);  // true = operate on predicted positions

    // Build spatial grid for neighbor search
    buildGrid();

    // Perform density constraint iterations
    std::vector<std::vector<int>> allNeighbors(particles.size());

    // Pre-compute all neighborhoods for efficiency
    for (size_t p = 0; p < particles.size(); ++p) {
        allNeighbors[p] = getNeighbors(particles[p].predictedPosition);
    }

    // Constraint solving loop
    for (int i = 0; i < solverIterations; ++i)
    {
        // Compute densities
        computeDensities(allNeighbors);

        // Compute lambdas (constraint multipliers)
        computeLambdas(allNeighbors);

        // Compute position corrections
        computePositionCorrections(allNeighbors);

        // Apply position corrections
        applyPositionCorrections();

        // Re-enforce boundaries after each correction step
        enforceBoxBoundaries(true);
    }

    // Update velocities based on position changes
    updateVelocities();

    // Apply vorticity confinement and XSPH viscosity
    applyVorticityConfinement(allNeighbors);
    applyXSPHViscosity(allNeighbors);

    // Final boundary check on actual positions
    enforceBoxBoundaries(false);
}

void PBFSystem::predictPositions()
{
    for (auto& p : particles)
    {
        // Apply gravity with limiting for stability
        p.velocity += gravity * dt;

        // Limit velocity to prevent explosive behavior
        float maxSpeed = 10.0f;
        float speedSq = glm::length2(p.velocity);
        if (speedSq > maxSpeed * maxSpeed) {
            p.velocity = glm::normalize(p.velocity) * maxSpeed;
        }

        // Predict new position
        p.predictedPosition = p.position + p.velocity * dt;
    }
}

void PBFSystem::buildGrid()
{
    grid.clear();

    // Insert particles into grid cells
    for (size_t i = 0; i < particles.size(); ++i)
    {
        glm::ivec3 cell = glm::ivec3(particles[i].predictedPosition / h);
        grid[cell].push_back(i);
    }
}

std::vector<int> PBFSystem::getNeighbors(const glm::vec3& position)
{
    std::vector<int> neighbors;
    neighbors.reserve(50);  // Pre-allocate space for efficiency

    glm::ivec3 centerCell = glm::ivec3(position / h);

    // Check neighboring cells (including center)
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
                glm::ivec3 cell = centerCell + glm::ivec3(x, y, z);

                auto it = grid.find(cell);
                if (it != grid.end())
                {
                    for (int index : it->second)
                    {
                        // Only include particles within kernel radius
                        float dist = glm::distance(position, particles[index].predictedPosition);
                        if (dist < h && dist > epsilon) // Avoid self at exact position
                        {
                            neighbors.push_back(index);
                        }
                    }
                }
            }
        }
    }

    return neighbors;
}

float PBFSystem::kernelPoly6(float r, float h)
{
    if (r > h) return 0.0f;

    float h2 = h * h;
    float h9 = h2 * h2 * h2 * h2 * h;
    float coeff = 315.0f / (64.0f * 3.14159f * h9);
    float term = h2 - r * r;
    return coeff * term * term * term;
}

glm::vec3 PBFSystem::kernelSpikyGrad(const glm::vec3& r, float h)
{
    float rlen = glm::length(r);
    if (rlen > h || rlen < epsilon) return glm::vec3(0.0f);

    float h6 = h * h * h * h * h * h;
    float coeff = -45.0f / (3.14159f * h6);
    float term = h - rlen;
    float scale = coeff * term * term / (rlen + epsilon); // Add epsilon to avoid division by zero

    return scale * r;  // No need to normalize
}

void PBFSystem::computeDensities(std::vector<std::vector<int>>& neighbors)
{
    for (size_t i = 0; i < particles.size(); ++i)
    {
        // Self contribution
        particles[i].density = kernelPoly6(0, h);

        // Add neighbor contributions
        for (int j : neighbors[i])
        {
            float r = glm::distance(
                particles[i].predictedPosition,
                particles[j].predictedPosition
            );
            particles[i].density += kernelPoly6(r, h);
        }

        // Add boundary contribution for particles near ground
        // This helps bottom particles maintain proper density
        if (particles[i].predictedPosition.y < h) {
            // Approximate boundary particles with a planar contribution
            float boundaryDist = particles[i].predictedPosition.y;
            if (boundaryDist < h) {
                // Simple plane contribution - can be more sophisticated
                particles[i].density += 0.5f * kernelPoly6(boundaryDist, h);
            }
        }

        // Scale by rest density
        particles[i].density *= restDensity;
    }
}

void PBFSystem::computeLambdas(std::vector<std::vector<int>>& neighbors)
{
    for (size_t i = 0; i < particles.size(); ++i)
    {
        // Compute density constraint: ρ/ρ₀ - 1 = 0
        float constraint = particles[i].density / restDensity - 1.0f;

        // Skip computation for particles with too-low density to prevent suction
        if (constraint < -0.1f)
        {
            particles[i].lambda = 0.0f;
            continue;
        }

        // Compute the gradient sum for lambda calculation
        float gradSum2 = 0.0f;
        glm::vec3 gradI(0.0f);

        for (int j : neighbors[i])
        {
            // Calculate gradient of density with respect to position
            glm::vec3 r = particles[i].predictedPosition - particles[j].predictedPosition;
            glm::vec3 grad = kernelSpikyGrad(r, h) / restDensity;

            // Add to sum of squared gradient magnitudes
            gradSum2 += glm::dot(grad, grad);

            // Accumulate gradient for self
            gradI += grad;
        }

        // Add self gradient to sum
        gradSum2 += glm::dot(gradI, gradI);

        // Compute lambda with relaxation
        if (gradSum2 < epsilon) {
            particles[i].lambda = 0.0f; // Avoid division by near-zero
        }
        else {
            particles[i].lambda = -constraint / (gradSum2 + relaxationFactor);
        }
    }
}

void PBFSystem::computePositionCorrections(std::vector<std::vector<int>>& neighbors)
{
    for (size_t i = 0; i < particles.size(); ++i)
    {
        glm::vec3 deltaPos(0.0f);

        for (int j : neighbors[i])
        {
            glm::vec3 r = particles[i].predictedPosition - particles[j].predictedPosition;
            float dist = glm::length(r);

            if (dist > epsilon)
            {
                // Compute artificial pressure (anti-clustering)
                float corr = 0.0f;
                if (artificalPressureK > 0.0f)
                {
                    float kernelValue = kernelPoly6(dist, h);
                    float kernelDeltaQ = kernelPoly6(artificialPressureDeltaQ, h);

                    if (kernelDeltaQ > epsilon)
                    {
                        corr = -artificalPressureK * pow(kernelValue / kernelDeltaQ, artificialPressureN);
                    }
                }

                // Combined lambda effect
                float lambdaSum = particles[i].lambda + particles[j].lambda;

                // Calculate position correction
                glm::vec3 gradSpiky = kernelSpikyGrad(r, h);
                deltaPos += (lambdaSum + corr) * gradSpiky;
            }
        }

        // Scale by rest density
        particles[i].deltaP = deltaPos / restDensity;

        // Limit maximum correction to prevent explosive behavior
        float maxDelta = h * 0.1f;
        float deltaLength = glm::length(particles[i].deltaP);
        if (deltaLength > maxDelta) {
            particles[i].deltaP = (particles[i].deltaP / deltaLength) * maxDelta;
        }
    }
}

void PBFSystem::applyPositionCorrections()
{
    for (auto& p : particles)
    {
        // Simply apply the already-limited correction
        p.predictedPosition += p.deltaP;
    }
}

void PBFSystem::updateVelocities()
{
    for (auto& p : particles)
    {
        // Compute velocity from position change
        p.velocity = (p.predictedPosition - p.position) / dt;

        // Apply velocity damping for better settling
        float damping = 0.98f;
        p.velocity *= damping;

        // Apply additional damping to vertical velocity for faster settling
        // This helps particles come to rest more quickly
        if (glm::abs(p.velocity.y) < 0.5f) {
            p.velocity.y *= 0.95f;  // Extra damping on low vertical velocities
        }

        // Update position
        p.position = p.predictedPosition;
    }
}

void PBFSystem::applyVorticityConfinement(std::vector<std::vector<int>>& neighbors)
{
    if (vorticityStrength <= 0.0f) return;

    std::vector<glm::vec3> vorticity(particles.size(), glm::vec3(0.0f));

    // Calculate vorticity (curl of velocity field)
    for (size_t i = 0; i < particles.size(); ++i)
    {
        for (int j : neighbors[i])
        {
            glm::vec3 r = particles[i].position - particles[j].position;
            glm::vec3 velDiff = particles[j].velocity - particles[i].velocity;

            vorticity[i] += glm::cross(velDiff, kernelSpikyGrad(r, h));
        }
    }

    // Apply vorticity confinement force
    for (size_t i = 0; i < particles.size(); ++i)
    {
        float vortMag = glm::length(vorticity[i]);

        if (vortMag > epsilon)
        {
            // Calculate gradient of vorticity magnitude
            glm::vec3 gradVort(0.0f);

            for (int j : neighbors[i])
            {
                glm::vec3 r = particles[i].position - particles[j].position;
                float vmag = glm::length(vorticity[j]);

                gradVort += vmag * kernelSpikyGrad(r, h);
            }

            if (glm::length(gradVort) > epsilon)
            {
                // Apply vorticity force
                glm::vec3 n = glm::normalize(gradVort);
                glm::vec3 force = vorticityStrength * glm::cross(n, vorticity[i]);

                // Apply force with limiting
                glm::vec3 deltaV = force * dt;
                float maxForce = 1.0f; // Limit maximum force

                if (glm::length(deltaV) > maxForce) {
                    deltaV = glm::normalize(deltaV) * maxForce;
                }

                particles[i].velocity += deltaV;
            }
        }
    }
}

void PBFSystem::applyXSPHViscosity(std::vector<std::vector<int>>& neighbors)
{
    if (xsphC <= 0.0f) return;

    std::vector<glm::vec3> velocityCorrection(particles.size(), glm::vec3(0.0f));

    // Calculate XSPH viscosity correction
    for (size_t i = 0; i < particles.size(); ++i)
    {
        for (int j : neighbors[i])
        {
            float r = glm::distance(particles[i].position, particles[j].position);

            // Weight by Poly6 kernel
            float w = kernelPoly6(r, h);

            // Velocity difference
            glm::vec3 velDiff = particles[j].velocity - particles[i].velocity;

            // Accumulate correction
            velocityCorrection[i] += velDiff * w;
        }
    }

    // Apply viscosity correction
    for (size_t i = 0; i < particles.size(); ++i)
    {
        particles[i].velocity += xsphC * velocityCorrection[i];
    }
}

void PBFSystem::enforceBoxBoundaries(bool predictedPos)
{
    for (auto& p : particles)
    {
        glm::vec3& pos = predictedPos ? p.predictedPosition : p.position;

        // Check and enforce boundaries in each dimension
        for (int dim = 0; dim < 3; ++dim)
        {
            // Bottom/lower boundary (floor)
            if (dim == 1 && pos[dim] < minBoundary[dim] + particleRadius) {
                // Position correction
                pos[dim] = minBoundary[dim] + particleRadius;

                if (!predictedPos) {
                    // More damping for floor collisions to help settling
                    p.velocity[dim] *= -boundaryDamping;

                    // If velocity is very small, eliminate it entirely
                    if (abs(p.velocity[dim]) < 0.1f) {
                        p.velocity[dim] = 0.0f;
                    }
                }
            }
            // Other boundaries
            else if (pos[dim] < minBoundary[dim] + particleRadius) {
                pos[dim] = minBoundary[dim] + particleRadius;

                if (!predictedPos) {
                    p.velocity[dim] *= -boundaryDamping;
                }
            }
            // Upper boundaries
            else if (pos[dim] > maxBoundary[dim] - particleRadius) {
                pos[dim] = maxBoundary[dim] - particleRadius;

                if (!predictedPos) {
                    p.velocity[dim] *= -boundaryDamping;
                }
            }
        }
    }
}