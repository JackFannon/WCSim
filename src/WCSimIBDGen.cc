#include "WCSimIBDGen.hh"
#include "WCSimDetectorConstruction.hh"
#include "WCSimPrimaryGeneratorAction.hh"
#include "WCSimPrimaryGeneratorMessenger.hh"
#include <CLHEP/Units/PhysicalConstants.h>
#include <CLHEP/Units/SystemOfUnits.h>
#include <G4ThreeVector.hh>
#include <G4Types.hh>

WCSimIBDGen::WCSimIBDGen(WCSimDetectorConstruction *myDC) : myDetector(myDC) {}

WCSimIBDGen::~WCSimIBDGen() {
    // Delete things here
}

void WCSimIBDGen::ReadSpectrum(G4String spectrum_name) {

    // Populate vectors of energies and fluxes from the file spectrum_name
    // The file should have two columns: energy (MeV) and flux (cm^2 s^-1 MeV^-1)

    // Open the file
    std::ifstream spectrum_file(spectrum_name);

    // Check that the file is open
    if (!spectrum_file.is_open()) {
        G4cout << "Error opening file " << spectrum_name << G4endl;
        exit(1);
    }

    // Read the file
    float e, f;
    while (spectrum_file >> e >> f) {
        energy.push_back(e);
        flux.push_back(f);
    }

    // Set the minimum and maximum energies
    e_min = energy.front();
    e_max = energy.back();
}

double WCSimIBDGen::InterpolateSpectrum(std::vector<float> ener_vec, std::vector<float> flux_vec, float ene) {

    // Interpolate the spectrum at the energies in ener_vec
    // The spectrum is given by the vectors ener_vec and flux_vec

    // Loop over the energies in energy
    for (size_t i = 1; i < ener_vec.size(); i++) {
        if (ene <= ener_vec[i]) {
            // Perform linear interpolation
            double e1 = ener_vec[i - 1];
            double e2 = ener_vec[i];
            double f1 = flux_vec[i - 1];
            double f2 = flux_vec[i];

            double interpolated_value = f1 + (f2 - f1) * (ene - e1) / (e2 - e1);
            return interpolated_value;
        }
    }

    // If energy is larger than the maximum energy in the spectrum, return the final flux value
    return flux_vec.back();
}

G4ThreeVector WCSimIBDGen::GenRandomPosition() {
    // Generate random neutrino position
    // Pick random position in detector
    // Generate a random number between -1 and 1
    double x_nu = myDetector->GetGeo_Dm(0) * (-1.0 + 2.0 * G4UniformRand());
    double y_nu = myDetector->GetGeo_Dm(1) * (-1.0 + 2.0 * G4UniformRand());
    double z_nu = myDetector->GetGeo_Dm(2) * (-1.0 + 2.0 * G4UniformRand());

    G4ThreeVector nu_pos;

    // Set the nu_pos vector to the values generated above
    nu_pos.setX(x_nu);
    nu_pos.setY(y_nu);
    nu_pos.setZ(z_nu);

    return nu_pos;
}

void WCSimIBDGen::GenEvent(G4ThreeVector &nu_dir, G4LorentzVector &neutrino, G4LorentzVector &positron,
                           G4LorentzVector &neutron) {

    // Generate random neutrino direction
    // Pick isotropic direction
    double theta_nu = acos(2.0 * G4UniformRand() - 1.0);
    double phi_nu = 2.0 * G4UniformRand() * CLHEP::pi;
    nu_dir.setRThetaPhi(1.0, theta_nu, phi_nu);
    std::cout << "nu dir: " << nu_dir << std::endl;

    // Pick energy of neutrino and relative direction of positron
    float e_nu, cos_theta;
    GenInteraction(e_nu, cos_theta);

    // Print e_nu
    std::cout << "e_nu: " << e_nu << std::endl;
    // cos theta
    std::cout << "cos theta: " << cos_theta << std::endl;

    // First order correction to positron quantities
    // for finite nucleon mass
    double e1 = PositronEnergy(e_nu, cos_theta);
    // Print positron energy
    std::cout << "positron energy: " << e1 << std::endl;
    double p1 = sqrt(e1 * e1 - CLHEP::electron_mass_c2 * CLHEP::electron_mass_c2);

    // Compute neutrino 4-momentum
    neutrino.setVect(nu_dir * e_nu);
    neutrino.setE(e_nu);

    // Compute positron 4-momentum
    G4ThreeVector pos_momentum(p1 * nu_dir);

    // Rotation from nu direction to pos direction.
    double theta = acos(cos_theta);
    double phi = 2 * CLHEP::pi * G4UniformRand(); // Random phi
    G4ThreeVector rotation_axis = nu_dir.orthogonal();
    rotation_axis.rotate(phi, nu_dir);
    pos_momentum.rotate(theta, rotation_axis);

    positron.setVect(pos_momentum);
    positron.setE(e1);

    // Compute neutron 4-momentum
    neutron.setVect(neutrino.vect() - positron.vect());
    neutron.setE(sqrt(neutron.vect().mag2() + CLHEP::neutron_mass_c2 * CLHEP::neutron_mass_c2));
}

void WCSimIBDGen::GenInteraction(float &rand_ene, float &rand_cos_theta) {
    G4bool passed = false;

    G4double xs_max = CrossSection(e_max, -1.0);

    while (!passed) {
        // Pick energy and directory uniformly
        rand_ene = e_min + (e_max - e_min) * G4UniformRand();
        rand_cos_theta = -1.0 + 2.0 * G4UniformRand();

        // Weight events by spectrum
        G4double xs_test = xs_max * flux_max * G4UniformRand();

        // Cross section
        G4double xs_weight = CrossSection(rand_ene, rand_cos_theta);

        // Flux at rand_ene
        G4double flux_weight = InterpolateSpectrum(energy, flux, rand_ene);

        passed = (xs_test < xs_weight * flux_weight);
    }
}

double WCSimIBDGen::CrossSection(double e_nu, double cos_theta) {

    const double cos_theta_c = (0.9741 + 0.9756) / 2.0;

    // Radiative correction constant
    const double rad_cor = 0.024;

    const double DELTA = CLHEP::neutron_mass_c2 - CLHEP::proton_mass_c2;

    // Neutrino energy threshold for inverse beta decay
    const double e_nu_min = ((CLHEP::proton_mass_c2 + DELTA + CLHEP::electron_mass_c2) *
                                 (CLHEP::proton_mass_c2 + CLHEP::electron_mass_c2 + DELTA) -
                             CLHEP::proton_mass_c2 * CLHEP::proton_mass_c2) /
                            2 / CLHEP::proton_mass_c2;

    if (e_nu < e_nu_min) {
        return 0.0;
    }

    const double GFERMI = 1.16639e-11;

    const double sigma_0 = GFERMI * GFERMI * cos_theta_c * cos_theta_c / CLHEP::pi * (1 + rad_cor);

    // Couplings
    const double f = 1.00;
    const double f2 = 3.706;
    const double g = 1.26;

    // Order 0 terms
    double e0 = e_nu - DELTA;
    if (e0 < CLHEP::electron_mass_c2) {
        e0 = CLHEP::electron_mass_c2;
    }
    double p0 = sqrt(e0 * e0 - CLHEP::electron_mass_c2 * CLHEP::electron_mass_c2);
    double v0 = p0 / e0;

    // Order 1 terms
    const double y_squared = (DELTA * DELTA - CLHEP::electron_mass_c2 * CLHEP::electron_mass_c2) / 2;
    double e1 = e0 * (1 - e_nu / CLHEP::proton_mass_c2 * (1 - v0 * cos_theta)) - y_squared / CLHEP::proton_mass_c2;
    if (e1 < CLHEP::electron_mass_c2) {
        e1 = CLHEP::electron_mass_c2;
    }
    double p1 = sqrt(e1 * e1 - CLHEP::electron_mass_c2 * CLHEP::electron_mass_c2);
    double v1 = p1 / e1;

    double gamma =
        2 * (f + f2) * g *
            ((2 * e0 + DELTA) * (1 - v0 * cos_theta) - CLHEP::electron_mass_c2 * CLHEP::electron_mass_c2 / e0) +
        (f * f + g * g) * (DELTA * (1 + v0 * cos_theta) + CLHEP::electron_mass_c2 * CLHEP::electron_mass_c2 / e0) +
        (f * f + 3 * g * g) * ((e0 + DELTA) * (1 - cos_theta / v0) - DELTA) +
        (f * f - g * g) * ((e0 + DELTA) * (1 - cos_theta / v0) - DELTA) * v0 * cos_theta;

    double cross_section = ((f * f + 3 * g * g) + (f * f - g * g) * v1 * cos_theta) * e1 * p1 -
                           gamma / CLHEP::proton_mass_c2 * e0 * p0;

    cross_section *= sigma_0 / 2;

    // Convert from MeV^{-2} to mm^2 (native units for GEANT4)
    cross_section *= CLHEP::hbarc * CLHEP::hbarc;

    return cross_section;
}

double WCSimIBDGen::PositronEnergy(double e_nu, double cos_theta) {
    // Returns positron energy with first order corrections
    // Zero'th order approximation of positron quantities - infinite nucleon mass
    double e0 = e_nu - CLHEP::neutron_mass_c2 + CLHEP::electron_mass_c2;
    double p0 = sqrt(e0 * e0 - CLHEP::electron_mass_c2 * CLHEP::electron_mass_c2);
    double v0 = p0 / e0;

    // First order correction to positron quantities -- see page 3 astro-ph/0302055
    const double y_squared =
        (CLHEP::neutron_mass_c2 - CLHEP::proton_mass_c2) * (CLHEP::neutron_mass_c2 - CLHEP::proton_mass_c2) / 2;
    double e1 = e0 * (1 - e_nu / CLHEP::proton_mass_c2 * (1 - v0 * cos_theta)) - y_squared / CLHEP::proton_mass_c2;
    if (e1 < CLHEP::electron_mass_c2) {
        e1 = CLHEP::electron_mass_c2;
    }
    return e1;
}
