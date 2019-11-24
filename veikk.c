#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/hid.h>
#include <linux/kfifo.h>
#include <linux/leds.h>
#include <linux/usb/input.h>
#include <linux/power_supply.h>
#include <asm/unaligned.h>

#define VEIKK_PKGLEN_MAX  361

// module parameters for the configuration utility
// located in sysfs parameters (/sys/module/veikk/parameters)
// see the README for more information
static int orientation = 0;
module_param(orientation, int, 0660);

static int bounds_map[4] = { 0, 0, 100, 100 };
static int count;
module_param_array(bounds_map, int, &count, 0660);
unsigned int tmp_x = 16000;
unsigned int tmp_y = 16000;
unsigned int std_x = 15000;
unsigned int std_y = 15000;
unsigned int pos_x = 1;
unsigned int pos_y = 1;

static int pressure_map = 0;
module_param(pressure_map, int, 0660);

static int Screen_or_Mouse = 0;
module_param(Screen_or_Mouse, int, 0660);
// struct for user interface
struct veikk_vei {
  struct input_dev *pen_input;
  struct input_dev *touch_input;
  struct input_dev *pad_input;
	  // TODO: can this be deleted? what is pen_fifo for?
	  //  will delete when able to test
  struct kfifo_rec_ptr_2 pen_fifo;
  unsigned char data[VEIKK_PKGLEN_MAX];
};

// struct for hardware interface
struct veikk {
  struct usb_device *usbdev;
  struct usb_interface *intf;
  struct veikk_vei veikk_vei; 
  struct hid_device *hdev;
};

static const struct hid_device_id id_table[] = {
  { HID_USB_DEVICE(0x2feb, 0x0001) },
  // TODO: confirm that 0x0002 is indeed the product ID for the A30
  { HID_USB_DEVICE(0x2feb, 0x0002) },
  { HID_USB_DEVICE(0x2feb, 0x0003) },
  { }
};
MODULE_DEVICE_TABLE(hid, id_table);

// other functions
void veikk_vei_irq(struct veikk_vei *veikk_vei, size_t len) {
  char *data = veikk_vei->data;
  struct input_dev *input = veikk_vei->pen_input;
  unsigned int x_out, x_raw, y_out, y_raw, pressure_out, pressure_raw;

  input_report_key(input, BTN_TOUCH, (data[1] & 0x01));
  input_report_key(input, BTN_STYLUS, (data[1] & 0x02));
  input_report_key(input, BTN_STYLUS2, (data[1] & 0x04));

  // calculate x, y, and pressure given parameters
  // orientation map
  x_raw = (data[3] << 8) | (unsigned char) data[2];
  y_raw = (data[5] << 8) | (unsigned char) data[4];
  switch(orientation) {
    // rotate right
    case 1:
      x_out = 32767 - y_raw;
      y_out = x_raw;
      break;
    // rotate 180
    case 2:
      x_out = 32767 - x_raw;
      y_out = 32767 - y_raw;
      break;
    // rotate left
    case 3:
      x_out = y_raw;
      y_out = 32767 - x_raw;
      break;
    // default rotation
    default:
    case 0:
      x_out = x_raw;
      y_out = y_raw;
  }

  // map to section of screen
  // right now bounds_map are in percents, so awkward calculation
  x_out = (x_out * (bounds_map[2] - bounds_map[0]) + 32767 * bounds_map[0]) / 100;
  y_out = (y_out * (bounds_map[3] - bounds_map[1]) + 32767 * bounds_map[1]) / 100;
  if(Screen_or_Mouse == 1){
    if(abs(x_out-tmp_x) > 500 || abs(y_out-tmp_y) > 500){
      tmp_x = x_out;
      tmp_y = y_out;
    }
    pos_x = tmp_x - x_out;
    pos_y = tmp_y - y_out;
    tmp_x = x_out;
    tmp_y = y_out;
    std_x -= pos_x;
    std_y -= pos_y;
  }
  else{
  std_x = x_out;
  std_y = y_out;
  }

  
  

  // pressure mapping
  // right now only hardcoded functions
  pressure_raw = (data[7] << 8) | (unsigned char) data[6];
  switch(pressure_map) {
    // constant function
    case 1:
      pressure_out = pressure_raw ? 4095 : 0;
      break;
    // power function (n=1/2, sqrt)
    case 2:
      // 90 is approx. sqrt(8192)
      pressure_out = 90 * int_sqrt(pressure_raw);
      break;
    // power function (n=2, square)
    case 3:
      pressure_out = pressure_raw * pressure_raw / 8191;
      break;
    // linear mapping (reduced intensity for full pressure, suggested by @artixnous)
    case 4:
      pressure_out = (pressure_raw < 6144) ? 4 * pressure_raw / 3 : 8191;
      break;
    // linear mapping
    case 0:
    default:
      pressure_out = pressure_raw;
  }

  input_report_abs(input, ABS_X, std_x);
  input_report_abs(input, ABS_Y, std_y);
  input_report_abs(input, ABS_PRESSURE, pressure_out);

  if(veikk_vei->pen_input)
    input_sync(veikk_vei->pen_input);
  if(veikk_vei->touch_input)
    input_sync(veikk_vei->touch_input);
  if(veikk_vei->pad_input)
    input_sync(veikk_vei->pad_input);
}
static int veikk_open(struct input_dev *dev) {
  struct veikk *veikk = input_get_drvdata(dev);

  return hid_hw_open(veikk->hdev);
}
static void veikk_close(struct input_dev *dev) {
  struct veikk *veikk = input_get_drvdata(dev);

  if(veikk->hdev)
    hid_hw_close(veikk->hdev);
}
int veikk_setup_pen_input_capabilities(struct input_dev *input_dev, struct veikk_vei *veikk_vei) {
  input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

  __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

  __set_bit(BTN_TOUCH, input_dev->keybit);
  __set_bit(BTN_STYLUS, input_dev->keybit);
  __set_bit(BTN_STYLUS2, input_dev->keybit);

  // TODO: these are hardcoded to fit the S640, adjust for later
  input_set_abs_params(input_dev, ABS_X, 0, 32767, 0, 0);
  input_set_abs_params(input_dev, ABS_Y, 0, 32767, 0, 0);
  input_set_abs_params(input_dev, ABS_PRESSURE, 0, 8191, 0, 0);

  // TODO: what does this value do? What should it be set to?
  //       I've seen 25, 100 as values; all seem to work equally well
  input_abs_set_res(input_dev, ABS_X, 1);
  input_abs_set_res(input_dev, ABS_Y, 1);

  return 0;
}

/* No touch(screen?) input capabilities on the S640
int veikk_setup_touch_input_capabilities(struct input_dev *input_dev, struct veikk_vei *veikk_vei) {

  input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

  __set_bit(INPUT_PROP_POINTER, input_dev->propbit);

  __set_bit(BTN_TOUCH, input_dev->keybit);

  input_set_abs_params(input_dev, ABS_X, 0, 32767, 0, 0);
  input_set_abs_params(input_dev, ABS_Y, 0, 32767, 0, 0);
  input_abs_set_res(input_dev, ABS_X, 100);   // TODO: get actual resolution (this is just a guess for now, not sure of actual value)
  input_abs_set_res(input_dev, ABS_Y, 100);

  return 0;
}
*/
static struct input_dev *veikk_allocate_input(struct veikk *veikk) {
  struct input_dev *input_dev;
  struct hid_device *hdev = veikk->hdev;

  input_dev = devm_input_allocate_device(&hdev->dev);
  if(!input_dev)
    return NULL;

  input_dev->name = "Veikk device"; // will be overwritten
  input_dev->phys = hdev->phys;
  input_dev->dev.parent = &hdev->dev;
  input_dev->open = veikk_open;
  input_dev->close = veikk_close;
  input_dev->uniq = hdev->uniq;
  input_dev->id.bustype = hdev->bus;
  input_dev->id.vendor = hdev->vendor;
  input_dev->id.product = hdev->product;
  input_dev->id.version = hdev->version;
  input_set_drvdata(input_dev, veikk);

  return input_dev;
}
static int veikk_allocate_inputs(struct veikk *veikk) {
  struct veikk_vei *veikk_vei = &(veikk->veikk_vei);

  // right now only a pen is used (S640 only uses pen events; touch and pad don't do anything)
  veikk_vei->pen_input = veikk_allocate_input(veikk);
  veikk_vei->touch_input = veikk_allocate_input(veikk);
  veikk_vei->pad_input = veikk_allocate_input(veikk);

  if(!veikk_vei->pen_input || !veikk_vei->touch_input || !veikk_vei->pad_input)
    return -ENOMEM;

  veikk_vei->pen_input->name = "Veikk A30 Pen";
  veikk_vei->touch_input->name = "Veikk Touch";
  veikk_vei->pad_input->name = "Veikk Pad";

  return 0;
}
static int veikk_register_inputs(struct veikk *veikk) {
  struct input_dev *pen_input_dev, *touch_input_dev, *pad_input_dev;
  struct veikk_vei *veikk_vei = &(veikk->veikk_vei);
  int error = 0;

  pen_input_dev = veikk_vei->pen_input;
  touch_input_dev = veikk_vei->touch_input;
  pad_input_dev = veikk_vei->pad_input;

  error = veikk_setup_pen_input_capabilities(pen_input_dev, veikk_vei);
  if(error) {
    input_free_device(pen_input_dev);
    veikk_vei->pen_input = NULL;
    pen_input_dev = NULL;
  } else {
    error = input_register_device(pen_input_dev);
    if(error)
      goto fail;
  }
  
  // TODO: is there a touch(screen?) on this thing?
  // I don't think this is being used rn
  input_free_device(touch_input_dev);
  veikk_vei->touch_input = NULL;
  touch_input_dev = NULL;

  // TODO: is there a pad on this thing?
  // I don't think this is being used rn
  input_free_device(pad_input_dev);
  veikk_vei->pad_input = NULL;
  pad_input_dev = NULL;
  
  return 0;

fail:
  printk(KERN_WARNING "Error from veikk_register_inputs(): %i", error);
  veikk_vei->pen_input = NULL;
  veikk_vei->touch_input = NULL;
  veikk_vei->pad_input = NULL;
  return error;
}
static int veikk_parse_and_register(struct veikk *veikk) {
  struct hid_device *hdev = veikk->hdev;
  int error;
  unsigned int connect_mask = HID_CONNECT_HIDRAW;

  if(!devres_open_group(&hdev->dev, veikk, GFP_KERNEL))
    return -ENOMEM;

  error = veikk_allocate_inputs(veikk);
  if(error)
    goto fail;

  error = veikk_register_inputs(veikk);
  if(error)
    goto fail;
  
  error = hid_hw_start(hdev, connect_mask);
  if(error) {
    hid_err(hdev, "hw start failed\n");
    goto fail;
  }

  devres_close_group(&hdev->dev, veikk);

  return 0;

fail:
  printk(KERN_WARNING "Error from veikk_parse_and_register(): %i", error);
  return error;
}

// module functions
static int veikk_probe(struct hid_device *hdev, const struct hid_device_id *id) {
  struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
  struct usb_device *dev = interface_to_usbdev(intf);
  struct veikk *veikk;
  struct veikk_vei *veikk_vei;
  int error;

  printk(KERN_INFO "Inside veikk_probe()");
  
  veikk = devm_kzalloc(&hdev->dev, sizeof(struct veikk), GFP_KERNEL);
  if(!veikk) {
    error = -ENODEV;
    goto fail;
  }

  hid_set_drvdata(hdev, veikk);
  veikk->hdev = hdev;
  veikk_vei = &veikk->veikk_vei;
  
  error = kfifo_alloc(&veikk_vei->pen_fifo, VEIKK_PKGLEN_MAX, GFP_KERNEL);
  if(error)
    goto fail;

  veikk->usbdev = dev;
  veikk->intf = intf;

  error = hid_parse(hdev);
  if(error) {
    hid_err(hdev, "parse failed\n");
    goto fail;
  }

  error = veikk_parse_and_register(veikk);
  if(error)
    goto fail;

  return 0;

fail:
  hid_set_drvdata(hdev, NULL);
  printk(KERN_WARNING "Error in veikk_probe(): %i", error);
  return error;

}
static void veikk_remove(struct hid_device *hdev) {
  struct veikk *veikk = hid_get_drvdata(hdev);
	struct veikk_vei *veikk_vei = &veikk->veikk_vei;

  printk(KERN_INFO "Inside veikk_remove()");

	hid_hw_stop(hdev);

	kfifo_free(&veikk_vei->pen_fifo);

	hid_set_drvdata(hdev, NULL);
}
static int veikk_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *raw_data, int size) {
  struct veikk *veikk = hid_get_drvdata(hdev);

	if (size > VEIKK_PKGLEN_MAX)
		return 1;

	memcpy(veikk->veikk_vei.data, raw_data, size);

  veikk_vei_irq(&veikk->veikk_vei, size);

  return 0;
}

static struct hid_driver veikk_driver = {
  .name       = "veikk",
  .id_table   = id_table,
  .probe      = veikk_probe,
  .remove     = veikk_remove,
  .raw_event  = veikk_raw_event
};
module_hid_driver(veikk_driver);

MODULE_LICENSE("GPL");
