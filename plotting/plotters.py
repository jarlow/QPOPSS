import pandas as pd
from math import ceil
import numpy as np

''' Constants '''
RUNTIME=4 #For how long did each repetition execute?

names = ["spacesaving single", "spacesaving deleg"]
fancy_names = ["Single Space-Saving","Delegation Space-Saving"]

datasets = ["", "caida_dst_ip", "caida_dst_port"]
fancy_dataset_names = ["Zipf", "CAIDA Dest. IPv4", "CAIDA Dest. Port"]

showplots_flag = True
saveplots_flag = False
''' ######### '''

def parse_throughput(filename):
    returnList = []  
    with open(filename,'r') as f:
        for line in f:
            perf = float(line.split()[3])
            returnList.append(perf)
    return returnList

def parse_accuracy_histogram(filename):
    ''' Reads a histogram result file and outputs a list of lists of the form ([true_rank,true_count,estimate_rank,estimate_count], streamlength, phi value )

    Parameters
    ----------
    filename : str  
        the name of the histogram file to be parsed

    Returns
    ----------
    3-tuple
        ([true_rank,true_count,estimate_rank,estimate_count], streamlength, phi value )
    '''
    file1 = open(filename, 'r')
    lines = file1.readlines()
    allLines = list()
    N, _, Phi = lines[0].split()
    for line in lines[1:]:
        content = line.split()
        allLines.append(content)
    return allLines, int(N), float(Phi)

def parse_memory(filename):
    file1 = open(filename, 'r')
    lines = file1.readlines()
    bytes, counters = lines[0].split()
    return bytes, counters

def parse_accuracy(filename):
    precs = []
    recs = []
    ares = []
    with open(filename, 'r') as f:
        for line in f:
            perf = line.split(", ")
            prec = float(perf[0].split(':')[1])
            rec = float(perf[1].split(':')[1])
            are = float(perf[2].split(':')[1])
            precs.append(prec)
            recs.append(rec)
            ares.append(are)
    return precs, recs, ares

def average_and_std(l):
    return np.average(l), np.std(l)
    '''
    np.average(l)
    np.std(l)
    ret_avg = []
    ret_std = []
    steps = int(len(l)/reps)
    index = 0
    for i in range(steps):
        segment = l[index:index+reps]
        ret_avg = np.average(segment)
        ret_std = np.std(segment)
        index += reps
    return ret_avg, ret_std
    '''

def phiprecision(res, N, PHI):
    ''' !Not used! calculates precision from returned set and ground truth histogram '''
    truth = [line[1] for line in res if int(
        line[2]) >= ceil(N*PHI)]  # take ceil() of N*phi
    elements = [line[3] for line in res if line[3] != '4294967295']
    true_positives = [e for e in elements if e in truth]
    return 1 if len(elements) == 0 else len(true_positives)/len(elements)

def phirecall(res, N, PHI):
    ''' !Not used! calculates recall from returned set and ground truth histogram '''
    truth = [line[1] for line in res if int(line[2]) >= ceil(N*PHI)]
    elements = [line[3] for line in res if line[3] != '4294967295']
    true_positives = [e for e in elements if e in truth]
    return 1 if len(elements) == 0 else len(true_positives)/len(truth)

def avg_relative_error(res, N, PHI):
    ''' !Not used! calculates average relative error from returned set and ground truth histogram '''
    avg_re = 0
    num_res = 0
    truth = [(line[1], int(line[2]))
             for line in res if int(line[2]) >= ceil(N*PHI)]
    elements = [(line[3], int(line[4]))
                for line in res if line[3] != '4294967295']
    for (elem, count) in elements:
        for (elem2, truecount) in truth:
            if elem == elem2:
                avg_re += abs(1-(count/truecount))
                num_res += 1
    return 0 if num_res == 0 else avg_re/num_res

def absolute_error(res, N, PHI):
    ''' Absolute error of a set of elements. 
    Returns one absolute error per tuple in the set.


    Parameters
    ----------
    res :  
        A set of (element,count) tuples
    
      '''
    df = pd.DataFrame(columns=['abs error', 'true_value'])
    for r in res:
        elem = r[3]
        if elem == '-1':
            break
        else:
            for t in res:
                if elem == t[1]:
                    truth = int(t[2])
                    estimate = int(r[4])
                    #are = abs(1-(estimate/truth)) #Absolute relative error
                    are =  abs(truth-estimate) #Absolute error
                    df.loc[elem] = [are, truth]
                    break
    df.sort_values('true_value', inplace=True, ascending=False)
    df.drop('true_value', axis=1, inplace=True)
    returnlist = []
    for index, row in df.iterrows():
        returnlist.append((index, row['abs error']))
    return returnlist

def format_float(num):
    return np.format_float_positional(num, trim='-')