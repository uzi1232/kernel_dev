cmd_/home/pi/git/kernel_dev/lkm_example/lkm_example.ko := ld -r  -EL -T ./scripts/module-common.lds -T ./arch/arm/kernel/module.lds  --build-id  -o /home/pi/git/kernel_dev/lkm_example/lkm_example.ko /home/pi/git/kernel_dev/lkm_example/lkm_example.o /home/pi/git/kernel_dev/lkm_example/lkm_example.mod.o ;  true