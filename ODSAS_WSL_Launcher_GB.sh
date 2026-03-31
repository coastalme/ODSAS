#!/bin/bash
#
# Script to run ODSAS as a loop over subfolder names in a user defined folder

# ----------------------------
# Replace these with the name of the executable 
# and the parameters it needs
							
# application name and arguments
export MYAPP=./odsas

# User defined folder to read feature numbers from subfolder names
# Change this path to point to your desired folder
ID_FOLDER="inline_fid"   # ogc_fid or inline_fid
FEATURES_FOLDER="in/Baselines/$ID_FOLDER"

# ---------------------------
# run the job
# ---------------------------
# Check if the features folder exists
if [ ! -d "$FEATURES_FOLDER" ]; then
  echo "Error: Features folder '$FEATURES_FOLDER' does not exist."
  exit 1
fi

# Get feature numbers from subfolder names (only directories, not files)
for SUBFOLDER in "$FEATURES_FOLDER"/*/; do
  # Check if any subdirectories exist
  if [ ! -d "$SUBFOLDER" ]; then
    echo "No subdirectories found in $FEATURES_FOLDER"
    exit 1
  fi
  
  # Extract the folder name (feature number)
  NUM=$(basename "$SUBFOLDER")
  
  echo "Running Feature: $NUM"
  
  # Check if the directory exists and is not empty
  if [ -d "out/$ID_FOLDER/$NUM" ] && [ "$(ls -A "out/$ID_FOLDER/$NUM" 2>/dev/null)" ]; then
    echo "Directory out/$ID_FOLDER/$NUM already exists and is not empty. Skipping feature $NUM."
    continue
  fi
  
  mkdir -p "out/$ID_FOLDER/$NUM" 
  
  sed -e "s/_#/$NUM/g" MyODSASINIfileGB.ini > ODSAS.ini
  #cat ODSAS.ini
  
  sed -e "s/_#/$NUM/g" MyODSASInputFileTemplateGB.dat > "out/$ID_FOLDER/$NUM/odsasinput_feature_$NUM.dat"
  #cat "out/$ID_FOLDER/$NUM/odsasinput_feature_$NUM.txt"
  
  echo "Will run command: $MYAPP using inputs for $ID_FOLDER feature: " $NUM 
  $MYAPP 

done
