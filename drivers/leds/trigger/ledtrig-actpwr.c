// SPDX-License-Identifier: GPL-2.0
/*
 * Activity/power trigger
 *
 * Copyright (C) 2020 Raspberry Pi (Trading) Ltd.
 *
 * Based on Atsushi Nemoto's ledtrig-heartbeat.c, although there may be
 * nothing left of the original now.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/leds.h>
#include "../leds.h"

enum {
	TRIG_ACT,
	TRIG_PWR,

	TRIG_COUNT
};

struct actpwr_trig_src {
	const char *name;
	int interval;
	bool invert;
};

struct actpwr_vled {
	struct led_classdev cdev;
	struct actpwr_trig_data *parent;
	enum led_brightness value;
	unsigned int interval;
	bool invert;
};

struct actpwr_trig_data {
	struct led_trigger trig;
	struct actpwr_vled virt_leds[TRIG_COUNT];
	struct actpwr_vled *active;
	struct timer_list timer;
	int next_active;
};

static int actpwr_trig_activate(struct led_classdev *led_cdev);
static void actpwr_trig_deactivate(struct led_classdev *led_cdev);

static const struct actpwr_trig_src actpwr_trig_sources[TRIG_COUNT] = {
	[TRIG_ACT] = { "mmc0", 500, true },
	[TRIG_PWR] = { "default-on", 500, false },
};

static struct actpwr_trig_data actpwr_data = {
	{
		.name     = "actpwr",
		.activate = actpwr_trig_activate,
		.deactivate = actpwr_trig_deactivate,
	}
};

static void actpwr_brightness_set(struct led_classdev *led_cdev,
				  enum led_brightness value)
{
	struct actpwr_vled *vled = container_of(led_cdev, struct actpwr_vled,
					       cdev);
	struct actpwr_trig_data *trig = vled->parent;

	if (vled->invert)
		value = !value;
	vled->value = value;

	if (vled == trig->active)
		led_trigger_event(&trig->trig, value);
}

static int actpwr_brightness_set_blocking(struct led_classdev *led_cdev,
					  enum led_brightness value)
{
	actpwr_brightness_set(led_cdev, value);
	return 0;
}

static enum led_brightness actpwr_brightness_get(struct led_classdev *led_cdev)
{
	struct actpwr_vled *vled = container_of(led_cdev, struct actpwr_vled,
					      cdev);

	return vled->value;
}

static void actpwr_trig_cycle(struct timer_list *t)
{
	struct actpwr_trig_data *trig  = &actpwr_data;
	struct actpwr_vled *active;

	active = &trig->virt_leds[trig->next_active];
	trig->active = active;
	trig->next_active = (trig->next_active + 1) % TRIG_COUNT;

	led_trigger_event(&trig->trig, active->value);

	mod_timer(&trig->timer, jiffies + msecs_to_jiffies(active->interval));
}

static int actpwr_trig_activate(struct led_classdev *led_cdev)
{
	struct actpwr_trig_data *trig  = &actpwr_data;

	/* Start the timer if this is the first LED */
	if (!trig->active)
		actpwr_trig_cycle(&trig->timer);
	else
		led_set_brightness_nosleep(led_cdev, trig->active->value);

	return 0;
}

static void actpwr_trig_deactivate(struct led_classdev *led_cdev)
{
	struct actpwr_trig_data *trig  = &actpwr_data;

	if (list_empty(&trig->trig.led_cdevs)) {
		del_timer_sync(&trig->timer);
		trig->active = NULL;
	}
}

static int __init actpwr_trig_init(void)
{
	struct actpwr_trig_data *trig  = &actpwr_data;
	int ret = 0;
	int i;

	timer_setup(&trig->timer, actpwr_trig_cycle, 0);

	/* Register one "LED" for each source trigger */
	for (i = 0; i < TRIG_COUNT; i++)
	{
		struct actpwr_vled *vled = &trig->virt_leds[i];
		struct led_classdev *cdev = &vled->cdev;
		const struct actpwr_trig_src *src = &actpwr_trig_sources[i];

		vled->parent = trig;
		vled->interval = src->interval;
		vled->invert = src->invert;
		cdev->name = src->name;
		cdev->brightness_set = actpwr_brightness_set;
		cdev->brightness_set_blocking = actpwr_brightness_set_blocking;
		cdev->brightness_get = actpwr_brightness_get;
		cdev->default_trigger = src->name;
		ret = led_classdev_register(NULL, cdev);
		if (ret)
			goto error_classdev;
	}

	ret = led_trigger_register(&trig->trig);
	if (ret)
		goto error_classdev;

	return 0;

error_classdev:
	while (i > 0)
	{
		i--;
		led_classdev_unregister(&trig->virt_leds[i].cdev);
	}

	return ret;
}

static void __exit actpwr_trig_exit(void)
{
	int i;

	led_trigger_unregister(&actpwr_data.trig);
	for (i = 0; i < TRIG_COUNT; i++)
	{
		led_classdev_unregister(&actpwr_data.virt_leds[i].cdev);
	}
}

module_init(actpwr_trig_init);
module_exit(actpwr_trig_exit);

MODULE_AUTHOR("Phil Elwell <phil@raspberrypi.com>");
MODULE_DESCRIPTION("ACT/PWR LED trigger");
MODULE_LICENSE("GPL v2");
