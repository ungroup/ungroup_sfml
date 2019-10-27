#include "CollisionUtil.hpp"
#include "VectorUtil.hpp"

bool CollisionUtil::areIntersecting(const CircleRigidBody& circle_a,
                                    const CircleRigidBody& circle_b) {
    if (circle_a.getId() == circle_b.getId()) {
        return false;
    }
    float dist = VectorUtil::distance(circle_a.getCenter(), circle_b.getCenter());
    return dist < circle_a.getRadius() + circle_b.getRadius();
}

/**
 * Returns infromation about the collision and how it should be resolved.
 * To resolve the collision between circle A and B, circle B should be moved in the direction
 * of the vector spanning from the center of cirlce A to the center of circle B by the
 * amount needed such that the circles are touching but not intersecting.
 */
Collision CollisionUtil::getCollision(const CircleRigidBody& circle_a,
                                      const CircleRigidBody& circle_b) {
    float distance = VectorUtil::distance(circle_a.getCenter(), circle_b.getCenter());
    float overlap = circle_a.getRadius() + circle_b.getRadius() - distance;
    sf::Vector2f normal =
        VectorUtil::normalize(VectorUtil::getVector(circle_b.getCenter(), circle_a.getCenter()));

    std::pair<sf::Vector2f, sf::Vector2f> resolution;
    if (circle_a.isMovable() && circle_b.isMovable()) {
        resolution = std::make_pair(normal * overlap / 2.f, -normal * overlap / 2.f);
    } else if (circle_a.isMovable() && !circle_b.isMovable()) {
        resolution = std::make_pair(normal * overlap, sf::Vector2f(0.f, 0.f));
    } else if (!circle_a.isMovable() && circle_b.isMovable()) {
        resolution = std::make_pair(sf::Vector2f(0.f, 0.f), -normal * overlap / 2.f);
    } else {
        resolution = std::make_pair(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f));
    }

    sf::Vector2f collision_point = VectorUtil::getMidpoint(
        circle_a.getCenter() + resolution.first, circle_b.getCenter() + resolution.second);

    Collision collision = {
        .ids = std::pair<uint32_t, uint32_t>(circle_a.getId(), circle_b.getId()),
        .position = collision_point,
        .normal = normal,
        .resolution = resolution,
    };

    return collision;
}

std::pair<PhysicsDef::Impulse, PhysicsDef::Impulse>
CollisionUtil::getImpulses(const CircleRigidBody& circle_a, const CircleRigidBody& circle_b,
                           const Collision& collision) {
    sf::Vector2f relative_velocity = circle_a.getVelocity() - circle_b.getVelocity();
    float impulse = -2.f * VectorUtil::dot(relative_velocity, collision.normal) /
                    (1 / circle_a.getMass() + 1 / circle_b.getMass());
    return std::make_pair((PhysicsDef::Impulse){impulse, collision.normal},
                          (PhysicsDef::Impulse){impulse, -collision.normal});
}