#include "collision_resolver.h"



void CollisionResolver::add_particles_to_grid()
{
    const int n = collision_bodies_->size();
    const int frame_parity = resolution_frame_ & 1;

    spatial_grid_.clear();

    int i = 0;
    for (Body* entity : *collision_bodies_)
    {
        sf::Vector2f pos = entity->position_;

        // only add this particle to the grid if it matches this frame's parity
        //if ((i & 1) == frame_parity)

        if (pos.x < 0)
            pos.x = 0;

        if (pos.y < 0)
            pos.y = 0;

        if (pos.x > spatial_grid_.world_width)
            pos.x = spatial_grid_.world_width;

        if (pos.y > spatial_grid_.world_height)
            pos.y = spatial_grid_.world_height;
        
        spatial_grid_.add_object(pos.x, pos.y, entity->id_);

        ++i;
    }
}

void CollisionResolver::update_particles_grid_indexes()
{
  
}