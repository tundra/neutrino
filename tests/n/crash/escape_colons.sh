set -e

# Scans through all the files in the directory and renames any that contain
# colons. Make really really doesn't like colons in filenames and AFL produces
# files with lots of colons.

ROOT=$(dirname $0)

COUNT=0
for OLD_NAME in ls $ROOT/*; do
  if [ -f $OLD_NAME ]; then
    NEW_NAME=$(echo $OLD_NAME | sed -e 's/[:,]/_/g')
    if [ "$OLD_NAME" != "$NEW_NAME" ]; then
      if [ -f "$NEW_NAME" ]; then
        read -p "Renamed file already exists. Delete $OLD_NAME? [yn] " CHOICE
        if [[ $CHOICE =~ ^[Yy]$ ]]; then
          rm $OLD_NAME
          COUNT=$(($COUNT + 1))
        else
          echo "Skipped deleting"
        fi
      else
        read -p "Rename $OLD_NAME to $NEW_NAME? [yn] " CHOICE
        if [[ $CHOICE =~ ^[Yy]$ ]]; then
          mv $OLD_NAME $NEW_NAME
          COUNT=$(($COUNT + 1))
        else
          echo "Skipped renaming"
        fi
      fi
    fi
  fi
done

echo "Files touched: $COUNT"
