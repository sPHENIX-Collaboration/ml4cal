#!/usr/bin/env python
'''
Explore EMCAL test data previously saved as numpy files

This version adapted to the new data format with
"evaluation tree" as the source ROOT format

The 32nd time bin of the waveform always contains -999 and is useless,
so by default we exclude it
'''
#
##################################
from os import ST_RDONLY

import  numpy as np
import  scipy
from    scipy.optimize import curve_fit

import  matplotlib.pyplot as plt
from    matplotlib import colors

import  argparse
import  platform
##################################

# Local packages:
import fits
from fits import funcz

##################################
peak_left, peak_right = (7,20)
##################################

parser = argparse.ArgumentParser()

parser.add_argument("-i", "--infile",   type=str,   help="Input numpy file",    default='')
parser.add_argument("-o", "--outfile",  type=str,   help="Output numpy file",   default='')

parser.add_argument("-l", "--left",     type=int,   help="Left limit",          default=0)
parser.add_argument("-r", "--right",    type=int,   help="Right limit",         default=31)

parser.add_argument("-L", "--bins",     type=int,   help="Number of bins",      default=31)
parser.add_argument("-N", "--Nevents",  type=int,   help="Number of events (zero for all)", default=0)
parser.add_argument("-n", "--nplot",    type=int,   help="Number of events to plot", default=10)

parser.add_argument("-t", "--threshold",    type=int,   help="Signal threshold", default=300)

parser.add_argument("-c", "--channel",    type=int,   help="Channel",           default=-1)

parser.add_argument("-v", "--verbose",  action='store_true',    help="Verbose mode")
################################################################
args        = parser.parse_args()

infile      = args.infile
outfile     = args.outfile

L           = args.bins
N           = args.Nevents


threshold   = args.threshold

nplot       = args.nplot
channel     = args.channel
verbose     = args.verbose

if(verbose):
    python_info = platform.python_version_tuple()
    print(f'''Python {python_info[0]}.{python_info[1]}.{python_info[2]}''')

if(infile==''):
    print('Please specify a valid input filename (input)')
    exit(-1)

with open(infile, 'rb') as f: X = np.load(f)
if verbose: print(f'''Read an array: {X.shape}''')
if N==0: N = X.shape[0] # how many events to process

x   = np.linspace(args.left, args.right, num=L-1, endpoint=False)
x1  = np.linspace(args.left, args.right, num=200, endpoint=False)

plt.style.use('seaborn-whitegrid')

new_length = L-1+5 # reserve 5 words for the training parameters

if (channel!=-1): 
    cnt_ped, cnt_sig = (0, 0)
    minima, maxima, x_min, x_max  = ([], [], [], [])

    fig, ((ax1, ax2, ax3), (ax4, ax5, ax6)) = plt.subplots(2,3)
    fig.set_size_inches(18.0, 12.0)
    fig.suptitle(f'''Channel {channel} pedestal and signal''')

    first           = True
    output_array    = None

    for i in range(N): # loop over the data sample
        frame = X[i]
        wave = frame[channel][0:L-1]
      
        pedestal = np.average(wave[0:5]) # print(wave) print(pedestal)
        std      = np.std(wave[0:5])
        if(pedestal<1700 and std<7):
            minima.append(pedestal)
            x_min.append(i)

        if(cnt_ped<nplot):
            ax3.plot(x, wave, 'o', linestyle='solid', linewidth=1.5,)
            cnt_ped+=1

        maxindex    =   np.argmax(wave)
        maxval      =   wave[maxindex]

        if(maxval-pedestal)<threshold: continue # skip events below trigger
        
        if maxindex<peak_left or maxindex>peak_right:
            # if verbose: print('Outlier: ', maxindex, wave[maxindex])
            continue

        maxima.append(wave[maxindex]-pedestal)
        x_max.append(i)
    
        guess = [float(maxindex), float(2*(maxval-pedestal)), 1.0, float(pedestal), 1.0]
    
        if(cnt_sig<nplot):
            ax6.plot(x, wave, 'o') # , linestyle='solid', linewidth=1.5,)
            # if(verbose): print(wave)
            cnt_sig+=1

            # guess = [maxindex, 2*(maxval-pedestal), pedestal, 1, 1.0]
            # guess = [10, 200, 10, 1500,1 ]

            try:
                popt, _ = scipy.optimize.curve_fit(funcz.landau, x, wave, p0=guess)
            except RuntimeError:
                if verbose: print('Fitting error')
                continue
            except RuntimeWarning:
                if verbose: print('Warning in the fitting function')
                continue
    
            # print('------------>', popt)
            # print(maxval, funcz.landau(popt[0], *popt))

            ax6.plot(x1, funcz.landau(x1, *popt))

            # residual sum of squares
            ss_res = np.sum((wave - funcz.landau(x, *popt)) ** 2)
            
            # total sum of squares
            ss_tot = np.sum((wave - np.mean(wave)) ** 2)
            
            # r-squared
            r2 = 1 - (ss_res / ss_tot)
            print('r2: ', r2)
  
        try:
            popt, _ = scipy.optimize.curve_fit(funcz.landau, x, wave, p0=guess)
        except RuntimeError:
            if verbose: print('Fitting error')
            continue
        except RuntimeWarning:
            if verbose: print('Warning in the fitting function')
            continue
    
        peak    = funcz.landau(popt[0], *popt)

        # adding the "Y" vector: origin, peak value, pedestal
        result  = np.array([popt[0], peak, popt[3]])
        
        appended = np.append(wave, result)

        if first:
            output_array = np.array([appended])
            first = False
        else:
            output_array = np.append(output_array,[appended], axis=0)

    if verbose: print(f'''Created an array: {output_array.shape}''')

    if(outfile!=''):
        with open(outfile, 'wb') as f_out:
            np.save(f_out, output_array)

        f_out.close()


    ax3.set_title('A sample of waveforms (no selection)')
    ax3.set_xlabel('Time bin')
    ax3.set_ylabel('ADC ch.')

    ax6.set_title(f'''A sample of waveforms (threshold={threshold})''')
    ax6.set_xlabel('Time bin')
    ax6.set_ylabel('ADC ch.')

    av_min, av_max = np.average(minima), np.average(maxima)

    _ = ax1.hist(minima, bins=50, color='deeppink', range=(av_min-15, av_min+15))
    ax1.set_title('Pedestal distribution')
    ax1.set_xlabel('ADC ch.')

    _ = ax2.hist(maxima, bins=20, align='left', range=(min(maxima), max(maxima)) )#, range=(1500, 17000))
    ax2.set_title('Signal-Pedestal distribution')
    ax2.set_xlabel('ADC ch.')



    _ = ax4.hist2d(minima, x_min, bins=(50,50), range=((av_min-15, av_min+15), (min(x_min), max(x_min))), norm=colors.LogNorm(1.0), cmap='PuRd') # PuRd
    ax4.set_title('Pedestal vs readout No.')
    ax4.set_xlabel('ADC ch.')
    ax4.set_ylabel('Readout No.')

    _ = ax5.hist2d(maxima, x_max, bins=(20,20), norm=colors.LogNorm(1.0), cmap='PuBuGn')
    ax5.set_title('Signal-Pedestal vs readout No.')
    ax5.set_xlabel('ADC ch.')
    ax5.set_ylabel('Readout No.')

    ax1.xaxis.set_zorder(10.0)
    ax1.yaxis.set_zorder(10.0)

    ax2.xaxis.set_zorder(10.0)
    ax2.yaxis.set_zorder(10.0)

    #ax4.grid()
    #ax4.xaxis.set_zorder(10.0)
    #ax4.yaxis.set_zorder(10.0)

    ax5.grid()

    plt.show()

exit(0)
