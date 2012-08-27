/*
 * EFI Runtime driver
 *
 * Copyright(C) 2012 Canonical Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/efi.h>

#include <linux/uaccess.h>

#include "efi_runtime.h"

#define EFI_FWTSEFI_VERSION	"0.1"

MODULE_AUTHOR("Ivan Hu");
MODULE_DESCRIPTION("EFI Runtime Driver");
MODULE_LICENSE("GPL");


static void convert_from_efi_time(efi_time_t *eft, EFI_TIME *time)
{
	memset(time, 0, sizeof(EFI_TIME));
	time->Year = eft->year;
	time->Month = eft->month;
	time->Day  = eft->day;
	time->Hour = eft->hour;
	time->Minute = eft->minute;
	time->Second  = eft->second;
	time->Pad1 = eft->pad1;
	time->Nanosecond = eft->nanosecond;
	time->TimeZone = eft->timezone;
	time->Daylight = eft->daylight;
	time->Pad2 = eft->pad2;

}

static void convert_to_efi_time(efi_time_t *eft, EFI_TIME *time)
{
	memset(eft, 0, sizeof(eft));
	eft->year = time->Year;
	eft->month = time->Month;
	eft->day = time->Day;
	eft->hour = time->Hour;
	eft->minute = time->Minute;
	eft->second = time->Second;
	eft->pad1 = time->Pad1;
	eft->nanosecond = time->Nanosecond;
	eft->timezone = time->TimeZone;
	eft->daylight = time->Daylight;
	eft->pad2 = time->Pad2;

}

static long efi_runtime_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	efi_status_t status;
	struct efi_getvariable getvariable;
	struct efi_setvariable setvariable;
	efi_time_t eft;
	efi_time_cap_t cap;
	struct efi_gettime gettime;
	struct efi_settime settime;

	struct efi_getwakeuptime getwakeuptime;
	unsigned char enabled, pending;
	struct efi_setwakeuptime setwakeuptime;

	switch (cmd) {
	case EFI_RUNTIME_GET_VARIABLE:
		if (copy_from_user(&getvariable,
					(struct efi_getvariable __user *)arg,
					sizeof(struct efi_getvariable)))
			return -EFAULT;

		status = efi.get_variable(getvariable.variable_name,
					(efi_guid_t *)getvariable.VendorGuid,
					getvariable.Attributes,
					getvariable.DataSize, getvariable.Data);
		if (status == EFI_SUCCESS) {
			return copy_to_user((void __user *)arg, &getvariable,
						sizeof(struct efi_getvariable))
						? -EFAULT : 0;
		} else {
			printk(KERN_ERR "efi_runtime: can't get variable\n");
			return -EINVAL;
		}
	case EFI_RUNTIME_SET_VARIABLE:
		if (copy_from_user(&setvariable,
					(struct efi_setvariable __user *)arg,
					sizeof(struct efi_setvariable)))
			return -EFAULT;

		status = efi.set_variable(setvariable.variable_name,
					(efi_guid_t *)setvariable.VendorGuid,
					setvariable.Attributes,
					setvariable.DataSize, setvariable.Data);

		return status == EFI_SUCCESS ? 0 : -EINVAL;
	case EFI_RUNTIME_GET_TIME:
		status = efi.get_time(&eft, &cap);
		if (status != EFI_SUCCESS) {
			printk(KERN_ERR "efitime: can't read time\n");
			return -EINVAL;
		}


		gettime.Capabilities.Resolution = cap.resolution;
		gettime.Capabilities.Accuracy = cap.accuracy;
		gettime.Capabilities.SetsToZero = cap.sets_to_zero;
		convert_from_efi_time(&eft, &gettime.Time);
		return copy_to_user((void __user *)arg, &gettime,
				sizeof(struct efi_gettime)) ? -EFAULT : 0;
	case EFI_RUNTIME_SET_TIME:

		if (copy_from_user(&settime, (struct efi_settime __user *)arg,
						sizeof(struct efi_settime)))
			return -EFAULT;
		convert_to_efi_time(&eft, &settime.Time);
		status = efi.set_time(&eft);
		return status == EFI_SUCCESS ? 0 : -EINVAL;

	case EFI_RUNTIME_GET_WAKETIME:

		status = efi.get_wakeup_time((efi_bool_t *)&enabled,
						(efi_bool_t *)&pending, &eft);

		if (status != EFI_SUCCESS)
			return -EINVAL;

		getwakeuptime.Enabled = (BOOLEAN)enabled;
		getwakeuptime.Pending = (BOOLEAN)pending;
		convert_from_efi_time(&eft, &getwakeuptime.Time);

		return copy_to_user((void __user *)arg, &getwakeuptime,
				sizeof(struct efi_getwakeuptime)) ? -EFAULT : 0;

	case EFI_RUNTIME_SET_WAKETIME:

		if (copy_from_user(&setwakeuptime,
					(struct efi_setwakeuptime __user *)arg,
					sizeof(struct efi_setwakeuptime)))
			return -EFAULT;

		convert_to_efi_time(&eft, &setwakeuptime.Time);

		status = efi.set_wakeup_time(setwakeuptime.Enabled, &eft);

		return status == EFI_SUCCESS ? 0 : -EINVAL;
	}

	return -ENOTTY;
}

static int efi_runtime_open(struct inode *inode, struct file *file)
{
	/*
	 * nothing special to do here
	 * We do accept multiple open files at the same time as we
	 * synchronize on the per call operation.
	 */
	return 0;
}

static int efi_runtime_close(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 *	The various file operations we support.
 */
static const struct file_operations efi_runtime_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= efi_runtime_ioctl,
	.open		= efi_runtime_open,
	.release	= efi_runtime_close,
	.llseek		= no_llseek,
};

static struct miscdevice efi_runtime_dev = {
	MISC_DYNAMIC_MINOR,
	"efi_runtime",
	&efi_runtime_fops
};

static int __init efi_runtime_init(void)
{
	int ret;

	printk(KERN_INFO "EFI_RUNTIME Driver v%s\n", EFI_FWTSEFI_VERSION);

	ret = misc_register(&efi_runtime_dev);
	if (ret) {
		printk(KERN_ERR "efi_runtime: can't misc_register on minor=%d\n",
				MISC_DYNAMIC_MINOR);
		return ret;
	}

	return 0;
}

static void __exit efi_runtime_exit(void)
{
	/* not yet used */
}

module_init(efi_runtime_init);
module_exit(efi_runtime_exit);

