#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "GameController.hpp"

#include "../events/CollisionEvent.hpp"
#include "../events/Event.hpp"
#include "../events/EventController.hpp"
#include "../factories/IdFactory.hpp"
#include "../util/game_def.hpp"
#include "../util/game_settings.hpp"

GameController::GameController(size_t max_player_count, size_t max_mine_count)
    : mPhysicsController(new PhysicsController()),
      mGameObjectStore(new GameObjectStore(mPhysicsController)) {
    mGameObjectStore->loadLevel(max_player_count, max_mine_count);

    mGroupController = std::unique_ptr<GroupController>(
        new GroupController(mGameObjectStore->getGroups(), mGameObjectStore->getPlayers()));
    mPlayerController =
        std::unique_ptr<PlayerController>(new PlayerController(mGameObjectStore->getPlayers()));
    mMineController =
        std::unique_ptr<MineController>(new MineController(mGameObjectStore->getMines()));

    mClock.restart();

    EventController::getInstance().addEventListener(
        EventType::EVENT_TYPE_COLLISION,
        std::bind(&GameController::collisionEvent, this, std::placeholders::_1));

    for (int i = 0; i < max_mine_count; i++) {
        mMineController->createMine();
    }
}

GameController::~GameController() { std::cout << "Deconstructing GameController" << std::endl; }

void GameController::update() {
    ClientInputs cis = collectInputs();

    // Take a variable amount of game state steps depending on how long the last frame took. See:
    // https://web.archive.org/web/20190403012130/https://gafferongames.com/post/fix_your_timestep/
    sf::Int32 frame_time = mClock.restart().asMilliseconds();
    mTimeAccumulator += frame_time;
    while (mTimeAccumulator >= MIN_TIME_STEP) {
        step(cis, MIN_TIME_STEP);
        mTimeAccumulator -= MIN_TIME_STEP;
        mElapsedTime += MIN_TIME_STEP;
    }

    setNetworkState();
}

void GameController::computeGameState(const ClientInputs& cis, sf::Int32 delta_ms) {
    updateGameObjects(cis);
    mPhysicsController->update(delta_ms);
    updateGameObjectsPostPhysics();
    EventController::getInstance().forceProcessEvents();
    incrementTick();
}

void GameController::updateGameObjects(const ClientInputs& cis) {
    mPlayerController->update(cis);
    mGroupController->update();
    mMineController->update();
}

void GameController::updateGameObjectsPostPhysics() {
    mPlayerController->updatePostPhysics();
    mGroupController->updatePostPhysics();
    mMineController->updatePostPhysics();
}

uint32_t GameController::createPlayerWithGroup(uint32_t client_id) {
    uint32_t new_player_id = mPlayerController->createPlayer(client_id);
    mGroupController->createGroup(new_player_id);
    return new_player_id;
}

void GameController::applyGameState(GameState game_state) {
    setTick(static_cast<unsigned int>(game_state.tick));
    for (auto gu : game_state.group_updates) {
        mGameObjectStore->getGroup(gu.group_id)->applyUpdate(gu);
    }
    for (auto mu : game_state.mine_updates) {
        mGameObjectStore->getMine(mu.mine_id)->applyUpdate(mu);
    }
    for (auto pu : game_state.player_updates) {
        mGameObjectStore->getPlayer(pu.player_id)->applyUpdate(pu);
    }
    mGroupController->applyUpdate(game_state.gcu);
}

GameState GameController::getGameState() {
    auto groups = mGameObjectStore->getGroups();
    auto mines = mGameObjectStore->getMines();
    auto players = mGameObjectStore->getPlayers();
    auto gcu = mGroupController->getUpdate();

    sf::Uint32 tick = static_cast<sf::Uint32>(getTick());
    std::vector<GroupUpdate> group_updates;
    std::vector<MineUpdate> mine_updates;
    std::vector<PlayerUpdate> player_updates;
    std::transform(groups.begin(), groups.end(), std::back_inserter(group_updates),
                   [](std::shared_ptr<Group> group) { return group->getUpdate(); });
    std::transform(mines.begin(), mines.end(), std::back_inserter(mine_updates),
                   [](std::shared_ptr<Mine> mine) { return mine->getUpdate(); });
    std::transform(players.begin(), players.end(), std::back_inserter(player_updates),
                   [](std::shared_ptr<Player> player) { return player->getUpdate(); });

    GameState gs = {tick, group_updates, mine_updates, player_updates, gcu};

    return gs;
}

void GameController::collisionEvent(std::shared_ptr<Event> event) {
    switch (event->getType()) {
        case EventType::EVENT_TYPE_COLLISION: {
            std::shared_ptr<CollisionEvent> collision_event =
                std::dynamic_pointer_cast<CollisionEvent>(event);

            // Handle collision
            Collision collision = collision_event->getCollision();
            GameObjectType collider_a_type =
                GameObjectType(IdFactory::getInstance().getType(collision.ids.first));
            GameObjectType collider_b_type =
                GameObjectType(IdFactory::getInstance().getType(collision.ids.second));
            if (collider_a_type == GameObjectType::group) {
                mGameObjectStore->getGroup(collision.ids.first)
                    ->applyForce(collision.direction * -30.f);
            }
            if (collider_b_type == GameObjectType::group) {
                mGameObjectStore->getGroup(collision.ids.second)
                    ->applyForce(collision.direction * 10.f);
            }
            break;
        }
        default: {
            std::cout << "Unexpected event type." << std::endl;
            break;
        }
    }
}