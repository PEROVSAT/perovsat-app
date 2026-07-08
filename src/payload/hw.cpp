#include "payload/hw.hpp"

/**
 * @file hw.cpp
 * @brief DeviceTree binding for payload hardware slots.
 *
 * Defines `face_z` and `face_x` as compile-time tables of device pointers and
 * presence flags derived from DeviceTree, without performing any I/O.
 */

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

namespace payload
{

#define CELL_SLOT(node_id)                                                                         \
	{                                                                                          \
		.dev = COND_CODE_1(DT_NODE_HAS_STATUS_OKAY(node_id), (DEVICE_DT_GET(node_id)),       \
				   (NULL)),                                 \
				  .present = DT_NODE_HAS_STATUS_OKAY(node_id),                     \
		}

/* TODO(ss200): replace with DT_NODELABEL(sun_z) / DT_NODELABEL(sun_x) once ss200-driver lands. */
#define SUN_SLOT_STUB                                                                              \
	{                                                                                          \
		.dev = NULL,                                                                       \
		.present = false,                                                                  \
	}

const FacePayload face_z = {
	.sun = SUN_SLOT_STUB,
	.ref = {CELL_SLOT(DT_NODELABEL(amu_z_ref0)), CELL_SLOT(DT_NODELABEL(amu_z_ref1))},
	.ps = {CELL_SLOT(DT_NODELABEL(amu_z_ps0)), CELL_SLOT(DT_NODELABEL(amu_z_ps1)),
	       CELL_SLOT(DT_NODELABEL(amu_z_ps2)), CELL_SLOT(DT_NODELABEL(amu_z_ps3)),
	       CELL_SLOT(DT_NODELABEL(amu_z_ps4)), CELL_SLOT(DT_NODELABEL(amu_z_ps5))},
};

const FacePayload face_x = {
	.sun = SUN_SLOT_STUB,
	.ref = {CELL_SLOT(DT_NODELABEL(amu_x_ref0)), CELL_SLOT(DT_NODELABEL(amu_x_ref1))},
	.ps = {CELL_SLOT(DT_NODELABEL(amu_x_ps0)), CELL_SLOT(DT_NODELABEL(amu_x_ps1)),
	       CELL_SLOT(DT_NODELABEL(amu_x_ps2)), CELL_SLOT(DT_NODELABEL(amu_x_ps3)),
	       CELL_SLOT(DT_NODELABEL(amu_x_ps4)), CELL_SLOT(DT_NODELABEL(amu_x_ps5))},
};

} // namespace payload
