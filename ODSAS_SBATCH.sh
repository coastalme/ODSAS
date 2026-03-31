#!/bin/bash
#
# Script to run ODSAS as a loop over subfolder names in a user defined folder

#SBATCH --job-name=ODSAS_Launcher 

#SBATCH --output=ODSAS_Launcher_output%j.txt 

#SBATCH --error=ODSAS_Launcher_error%j.txt 

#SBATCH --nodes=1              # Request 1 nodes 

#SBATCH --ntasks-per-node=1    # Number of  tasks/cores per node 

#SBATCH --cpus-per-task=1 

##SBATCH --time=10:00:00 

#SBATCH --partition=all 

#SBATCH --mail-type=ALL 

#SBATCH --mail-user=agarcia@bgs.ac.uk 

./ODSAS_WSL_Launcher_GB.sh