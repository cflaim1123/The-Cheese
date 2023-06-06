% Script to find all pressure values, didnt know how to do this in python
clear 
close all


%% Read Data 

data = readtable('data.csv');

pressures = table2array(data(:,2));
pic = table2array(data(:,5));
times = table2array(data(:,1));


%% Identifying indicies of photos
photos = find(pic);

pic_press = [];

pic_t = [];

for i = photos
    P = pressures(i);
    pic_press = [pic_press,{P}];
    t  = times(i);
    pic_t = [pic_t,{t}];
end

