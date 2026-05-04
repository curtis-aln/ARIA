#include "../Protozoa.h"

#include "../genetics/CellGenome.h"

#include <vector>

#include "Utils/Graphics/OrganicColorMutator.h"


void Protozoa::mutate(const bool artificial_add_cell, const float artificial_mutatation_rate, const float artificial_mutatation_range)
{
    // First we mutate the genome, which will update the gene values for each cell and spring, as well as the colors and mutation settings
    mutate_existing_cells(artificial_mutatation_rate, artificial_mutatation_range);
    mutate_existing_springs(artificial_mutatation_rate, artificial_mutatation_range);

	if (std::max(artificial_mutatation_rate, artificial_mutatation_range) == 0.f)
		return; // probally wants a clone with no mutations, so we skip the rest of the mutation process which adds or removes cells and springs

    // Check if we should add or remove a cell based on the genome's mutation logic
    const bool add_cell_ = Random::rand01_float() < CellGenome::add_cell_chance;
    const bool remove_cell_ = Random::rand01_float() < CellGenome::remove_cell_chance;
	const bool add_spring_ = Random::rand01_float() < CellGenome::add_spring_chance;
	const bool remove_spring_ = Random::rand01_float() < CellGenome::remove_spring_chance;


    if (add_cell_ || artificial_add_cell)
        add_cell();

    if (remove_cell_)
		remove_cell();

    if (add_spring_)
        add_spring();

    if (remove_spring_)
		remove_spring();
}

void Protozoa::mutate_existing_cells(float mut_rate, float mut_range)
{
    for (Cell& cell : m_cells_)
        cell.mutate(mut_rate, mut_range);
}

void Protozoa::mutate_existing_springs(float mut_rate, float mut_range)
{
    for (Spring& spring : m_springs_)
        spring.mutate(mut_rate, mut_range);
}


void Protozoa::add_cell()
{
    if (m_cells_.size() == max_cells)
		return;

    // finding the parent which will undergo mitosis
    const uint8_t parent_index = Random::rand_range(uint8_t(0), static_cast<uint8_t>(m_cells_.size() - 1));
	Cell& parent = m_cells_[parent_index];

    // creating the new cell and adding it to our cells
    Cell child{};
    child.id = m_cells_.size();
    parent.create_offspring(&child, true, false);
    m_cells_.push_back(child);

    // creating a spring connection to that cell
    const auto spring_id = static_cast<uint8_t>(m_springs_.size());
    Spring new_spring{ spring_id, parent_index, child.id };
    m_springs_.push_back(new_spring);
}

void Protozoa::remove_cell()
{
    if (m_cells_.size() <= 1)
        return;

    const auto cell_id = Random::rand_range(size_t(0), m_cells_.size() - 1);
    const int last_id = static_cast<int>(m_cells_.size() - 1);

    // remove all springs connected to the cell being removed
    for (size_t i = 0; i < m_springs_.size(); ++i)
    {
        if (m_springs_[i].cell_A_id == cell_id || m_springs_[i].cell_B_id == cell_id)
        {
            std::swap(m_springs_[i], m_springs_.back());
            m_springs_.pop_back();
            --i;
        }
    }

    // if the cell being removed isn't already the last one, we swap it with the last
    // and update any springs that were pointing to last_id to now point to cell_id
    if (static_cast<int>(cell_id) != last_id)
    {
        for (Spring& spring : m_springs_)
        {
            if (spring.cell_A_id == last_id) spring.cell_A_id = cell_id;
            if (spring.cell_B_id == last_id) spring.cell_B_id = cell_id;
        }

        std::swap(m_cells_[cell_id], m_cells_.back());
        m_cells_[cell_id].id = cell_id; // fix the id after the swap
    }

    m_cells_.pop_back();
}

void Protozoa::add_spring()
{
    // This is the simplest by far, choose two random cells, and connect them with a sprin
    if (m_cells_.size() < 2)
		return;

    // choose two different cell ids
	int cell_A_id = Random::rand_range(size_t(0), m_cells_.size() - 1);
	int cell_B_id = Random::rand_range(size_t(0), m_cells_.size() - 1);

    if (cell_A_id == cell_B_id)
		return; // better luck next time

    // check if a spring already exists between these two cells, if so we don't add another one
    for (const Spring& spring : m_springs_)
    {
        if ((spring.cell_A_id == cell_A_id && spring.cell_B_id == cell_B_id) ||
            (spring.cell_A_id == cell_B_id && spring.cell_B_id == cell_A_id))
        {
            return; // spring already exists
        }
    }

    const auto spring_id = static_cast<int>(m_springs_.size());
	m_springs_.emplace_back(spring_id, cell_A_id, cell_B_id);
}

void Protozoa::remove_spring(int8_t spring_id)
{
    if (m_springs_.empty())
        return;

    if (spring_id == -1) 
    {
        spring_id = Random::rand_range(size_t(0), m_springs_.size() - 1);
    }
    const auto end_id = m_springs_.size() - 1;

    // removing the spring through swap and pop
    std::swap(m_springs_[spring_id], m_springs_[end_id]); // swap

	// adjusting the id of the swapped spring
	m_springs_[spring_id].id = spring_id;

    m_springs_.pop_back(); // pop
}

